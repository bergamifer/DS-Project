# DS Beamforming - Plan de Testing y Desarrollo

**Fecha:** Enero 2026
**Estado:** En desarrollo inicial
**Hardware Disponible:** WeAct STM32H743, ESP32-S2 Mini
**Limitaciones:** Sin osciloscopio ni equipamiento de laboratorio durante 4 semanas

---

## 📋 Índice

- [Logros Alcanzados](#-logros-alcanzados)
- [Fase 1: Fundamentos STM32 (Semana 1)](#-fase-1-fundamentos-stm32-semana-1)
- [Fase 2: Comunicación ESP32-STM32 (Semana 2)](#-fase-2-comunicación-esp32-stm32-semana-2)
- [Fase 3: Sistema de Audio Virtual (Semana 3)](#-fase-3-sistema-de-audio-virtual-semana-3)
- [Fase 4: Validación de Hardware PCB (Semana 4)](#-fase-4-validación-de-hardware-pcb-semana-4)
- [Decisión: Fabricación PCB](#-decisión-fabricación-pcb)
- [Roadmap Post-Fabricación](#-roadmap-post-fabricación)

---

## ✅ Logros Alcanzados

### Documentación Técnica Completa
- ✅ **[DS_Hardware_Map.md](DS_Hardware_Map.md)** - Mapeo completo de pines y periféricos del STM32H743
- ✅ **[docs/audio_architecture.md](docs/audio_architecture.md)** - Arquitectura de audio con SAI, DMA y sincronización de clocks
- ✅ **[docs/pcm1802_registers.md](docs/pcm1802_registers.md)** - Documentación completa del ADC PCM1802
- ✅ **[docs/pcm1690_ref.md](docs/pcm1690_ref.md)** - Documentación de los DACs PCM1690
- ✅ **[stm32/](stm32/)** - Configuración detallada de RCC, SAI, DMA/BDMA, I2C, GPIO
- ✅ **[PowerSpecs.md](PowerSpecs.md)** - Análisis de consumo y fuente de alimentación
- ✅ **[memory_map.md](memory_map.md)** - Distribución de memoria RAM del STM32H743
- ✅ **[delays_calculation.md](delays_calculation.md)** - Cálculos matemáticos para beamforming

### Hardware
- ✅ **Diseño PCB completo** - Verificado contra netlist DS.net
- ✅ **Pinout validado** - Todos los pines SAI, I2C, UART, GPIO verificados
- ✅ **Sincronización de clocks** - MCLK compartido entre ADC y DACs confirmado
- ✅ **Layout crítico** - Resistores serie, pull-ups, decoupling caps especificados
- ⚠️ **Pendiente fabricación** - Esperando decisión final

### Webserver de Control
- ✅ **Backend Flask** - [webserver/app.py](webserver/app.py) funcional
- ✅ **Frontend HTML/CSS/JS** - Interfaz web para control de hasta 8 dispositivos DS
- ✅ **Cálculos beamforming** - Delays, ganancias, compensación de radiación
- ✅ **Base de datos SQLite** - Persistencia de configuraciones
- ✅ **Comunicación HTTP** - Protocolo definido para envío de comandos a ESP32
- ✅ **Documentación** - [WebServer.md](WebServer.md) completa

### Firmware STM32
- ✅ **Proyecto STM32CubeIDE** - [firmware/DS-Firmware.ioc](firmware/DS-Firmware.ioc) generado
- ⚠️ **Configuración inicial** - Pendiente implementación de código
- ⚠️ **Clocks configurados** - Pendiente ajuste fino de PLL para 12.288 MHz
- ⚠️ **Periféricos inicializados** - Pendiente SAI, DMA, I2C

---

## 🔧 Fase 1: Fundamentos STM32 (Semana 1)

**Objetivo:** Dominar el entorno de desarrollo y configurar los periféricos básicos del STM32H743 sin necesidad de equipamiento externo.

### 1.1 Setup del Proyecto (Día 1-2)

**Tareas:**
- [x] Instalar STM32CubeIDE (ya hecho)
- [ ] Configurar proyecto en CubeIDE desde [firmware/DS-Firmware.ioc](firmware/DS-Firmware.ioc)
- [ ] Compilar y flashear primer programa ("Hello World" por UART)
- [ ] Configurar ST-Link debugger
- [ ] Familiarizarse con debugging (breakpoints, step-through, watch variables)

**Verificación sin osciloscopio:**
```c
// En main.c - Enviar mensajes por UART
HAL_UART_Transmit(&huart3, "STM32 Initialized\r\n", 19, 100);
```
- Conectar USB-UART converter (3.3V) a PA9/PA10 (USART1) o usar ST-Link Virtual COM
- Abrir terminal serie (115200 baud) y verificar mensajes
- Usar `printf()` con retargeting a UART para debugging

**Recurso de aprendizaje:**
- Tutorial: "STM32 UART printf debugging" - Digikey
- RM0433 Section 50: USART

### 1.2 Configuración de Clocks (Día 3-4)

**Objetivo:** Configurar PLL para generar 12.288 MHz exactos para audio.

**Tareas:**
- [ ] Estudiar [stm32/clock_config.md](stm32/clock_config.md)
- [ ] Configurar HSE (25 MHz crystal de la WeAct board)
- [ ] Configurar PLL3 para SAI:
  ```
  HSE = 25 MHz
  DIVM3 = 25  → 1 MHz ref
  MULN3 = 344 → 344 MHz VCO
  DIVP3 = 28  → 12.2857 MHz (error 0.019%)
  ```
- [ ] Implementar función de verificación de clocks

**Verificación sin osciloscopio:**
```c
// Usar MCO (Microcontroller Clock Output) en PA8
HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_PLL3PCLK, RCC_MCODIV_1);
// Medir con multímetro la frecuencia aproximada (método AC coupling)
// O usar LED parpadeante basado en timer derivado del clock
```

**Método de verificación indirecta:**
- Configurar TIM2 con prescaler derivado de PLL3
- Hacer parpadear LED a frecuencia conocida (1 Hz)
- Cronometrar 100 parpadeos con reloj → Si tarda exactamente 100s, clock correcto

**Recurso de aprendizaje:**
- RM0433 Section 7: RCC
- AN4661: PLL configuration on STM32

### 1.3 GPIO y Control Básico (Día 5-6)

**Tareas:**
- [ ] Configurar todos los GPIOs de control según [DS_Hardware_Map.md](DS_Hardware_Map.md):
  - PA1 (ADC_PDWN), PA6 (DAC0_RST), PA7 (DAC1_RST)
  - PB0 (ADC_OSR), PB1 (DAC1_AMUTEI), PB5 (LED_STATUS)
  - PC3 (ADC_BYPASS), PD1 (DAC0_AMUTEI)
- [ ] Implementar funciones de inicialización
- [ ] Crear estado de "power-on sequence" simulado

**Verificación:**
```c
// Usar LED_STATUS (PB5) para indicar estados
void Signal_Init_Step(uint8_t step) {
    for (int i = 0; i < step; i++) {
        HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
        HAL_Delay(100);
    }
}

// Secuencia de inicialización
Signal_Init_Step(1); // Clock OK
Signal_Init_Step(2); // GPIOs OK
Signal_Init_Step(3); // SAI OK
```

### 1.4 I2C Básico (Día 7)

**Objetivo:** Configurar I2C1 para futuro control de DACs/ADC.

**Tareas:**
- [ ] Configurar I2C1 en PB8/PB9 (100 kHz, Fast Mode optional)
- [ ] Implementar scanner de dispositivos I2C
- [ ] Crear funciones de lectura/escritura genéricas

**Verificación sin chips I2C:**
```c
// I2C Scanner - Detecta dispositivos en el bus
for (uint8_t addr = 1; addr < 128; addr++) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK) {
        printf("Found I2C device at 0x%02X\r\n", addr);
    }
}
```
- Como no tenés los chips conectados, esto debería retornar vacío
- Verificar que no haya errores de timeout o bus stuck

**Simulación de lectura/escritura:**
- Crear array simulado que actúe como "chip virtual"
- Verificar correctitud del protocolo

**Recurso de aprendizaje:**
- RM0433 Section 49: I2C
- Ejemplo: STM32CubeH7/Examples/I2C

---

## 🔗 Fase 2: Comunicación ESP32-STM32 (Semana 2)

**Objetivo:** Establecer comunicación bidireccional robusta entre ESP32 y STM32 via UART7.

### 2.1 Configuración UART7 (Día 8-9)

**Tareas:**
- [ ] Configurar UART7 en PA8 (RX) / PA15 (TX)
- [ ] Baudrate: 115200 bps (estándar) o 921600 bps (rápido)
- [ ] Modo 8N1 (8 data bits, No parity, 1 stop bit)
- [ ] Habilitar interrupciones o DMA para recepción

**Conexión física:**
```
ESP32-S2          STM32H743
  TX  ───R220──►  PA8 (UART7_RX)
  RX  ◄──R220───  PA15 (UART7_TX)
  GND ──────────  GND
```

**Verificación:**
```c
// STM32: Echo test
uint8_t rx_data;
HAL_UART_Receive(&huart7, &rx_data, 1, 1000);
HAL_UART_Transmit(&huart7, &rx_data, 1, 100);

// ESP32 (Arduino): Enviar caracteres y verificar echo
Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
Serial2.write('A');
char echo = Serial2.read(); // Debería recibir 'A'
```

### 2.2 Protocolo de Comunicación (Día 10-11)

**Diseño del protocolo binario:**

```
Formato de paquete:
┌──────┬──────┬─────────┬──────────┬──────┐
│ STX  │ LEN  │ CMD     │ PAYLOAD  │ CRC  │
├──────┼──────┼─────────┼──────────┼──────┤
│ 0x02 │ 1B   │ 1B      │ N bytes  │ 2B   │
└──────┴──────┴─────────┴──────────┴──────┘

Comandos:
- 0x01: SET_MASTER_GAIN (payload: float 4B)
- 0x02: SET_CHANNEL_DELAY (payload: ch_num 1B + delay 2B)
- 0x03: SET_CHANNEL_GAIN (payload: ch_num 1B + gain 4B)
- 0x04: MUTE_CHANNEL (payload: ch_num 1B + mute 1B)
- 0x05: UPDATE_ALL (payload: config struct)
- 0x10: STATUS_REQUEST (payload: none)
- 0x11: STATUS_RESPONSE (payload: status struct)

Respuesta STM32:
┌──────┬──────┬─────────┬──────────┬──────┐
│ ACK  │ LEN  │ STATUS  │ DATA     │ CRC  │
├──────┼──────┼─────────┼──────────┼──────┤
│ 0x06 │ 1B   │ 1B      │ N bytes  │ 2B   │
└──────┴──────┴─────────┴──────────┴──────┘

STATUS:
- 0x00: OK
- 0x01: CRC_ERROR
- 0x02: INVALID_CMD
- 0x03: PAYLOAD_ERROR
```

**Tareas:**
- [ ] Implementar parser de paquetes en STM32
- [ ] Implementar validación CRC16-CCITT
- [ ] Crear struct para almacenar configuración
- [ ] Implementar respuestas ACK/NACK

**Código base:**
```c
typedef struct {
    uint8_t stx;
    uint8_t len;
    uint8_t cmd;
    uint8_t payload[256];
    uint16_t crc;
} UART_Packet_t;

typedef struct {
    float master_gain;
    uint16_t channel_delays[16];
    float channel_gains[16];
    uint8_t channel_mutes[16];
} Audio_Config_t;

HAL_StatusTypeDef UART_ParsePacket(UART_Packet_t *pkt);
void UART_SendResponse(uint8_t status, uint8_t *data, uint8_t len);
```

### 2.3 Firmware ESP32 (Día 12-13)

**Crear proyecto separado:** `~/STM32CubeIDE/DS-ESP32-Firmware/`

**Tareas:**
- [ ] Configurar ESP32-S2 en Arduino IDE o ESP-IDF
- [ ] Conectar WiFi a red local
- [ ] Implementar servidor HTTP simple (puerto 80)
- [ ] Crear endpoint `/command` que recibe JSON del webserver Flask
- [ ] Parsear JSON y convertir a paquetes UART binarios
- [ ] Enviar paquetes al STM32 vía UART
- [ ] Recibir ACK del STM32 y retornar status HTTP

**Código base ESP32:**
```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

WebServer server(80);

void handleCommand() {
    String body = server.arg("plain");
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, body);

    // Extraer parámetros
    float master_gain = doc["master_gain"];
    JsonArray channels = doc["channels"];

    // Construir paquete UART
    uint8_t packet[256];
    packet[0] = 0x02; // STX
    packet[1] = payload_len;
    packet[2] = CMD_UPDATE_ALL;
    // ... llenar payload
    uint16_t crc = calculate_crc16(packet, len-2);
    // ... agregar CRC

    // Enviar al STM32
    Serial2.write(packet, packet_len);

    // Esperar ACK
    uint8_t ack[10];
    if (Serial2.readBytes(ack, 4) == 4 && ack[0] == 0x06) {
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server.send(500, "application/json", "{\"status\":\"error\"}");
    }
}

void setup() {
    Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    WiFi.begin(SSID, PASSWORD);
    server.on("/command", HTTP_POST, handleCommand);
    server.begin();
}
```

### 2.4 Test End-to-End (Día 14)

**Objetivo:** Verificar comunicación completa Webserver → ESP32 → STM32

**Setup:**
1. ESP32 conectado a WiFi (anotar IP: ej. 192.168.1.231)
2. ESP32 conectado a STM32 via UART
3. Webserver Flask corriendo en Mac
4. Configurar IP del ESP32 en webserver

**Test de flujo completo:**
```bash
# En Mac - Enviar comando desde webserver
curl -X POST http://192.168.1.231/command \
  -H "Content-Type: application/json" \
  -d '{
    "command": "update_config",
    "master_gain": 0.8,
    "channels": [
      {"channel_num": 0, "delay_samples": 100, "channel_gain": 1.0}
    ]
  }'
```

**Verificación en STM32:**
```c
// En callback de recepción UART7
void UART7_RxCpltCallback(UART_HandleTypeDef *huart) {
    UART_ParsePacket(&rx_packet);
    if (rx_packet.cmd == CMD_UPDATE_ALL) {
        memcpy(&audio_config, rx_packet.payload, sizeof(Audio_Config_t));
        printf("Config updated: gain=%.2f\r\n", audio_config.master_gain);
        UART_SendResponse(STATUS_OK, NULL, 0);

        // Parpadear LED para indicar recepción exitosa
        HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
    }
}
```

**Métricas a medir:**
- Latencia end-to-end (Flask → STM32): debería ser < 50ms
- Tasa de error de paquetes: debería ser 0%
- Throughput: cuántos comandos/segundo soporta

---

## 🎵 Fase 3: Sistema de Audio Virtual (Semana 3)

**Objetivo:** Configurar SAI, DMA y buffers de audio SIN tener los chips físicos conectados. Usar datos sintéticos para verificar funcionamiento.

### 3.1 Configuración SAI (Día 15-17)

**Tareas:**
- [ ] Configurar SAI1_A (DAC0) según [stm32/sai_config.md](stm32/sai_config.md):
  - Master mode, Transmit
  - I2S protocol, 24-bit data, 32-bit frame
  - MCLK = 12.288 MHz, FS = 48 kHz
  - Pines: PE2 (MCLK), PE4 (FS), PE5 (SCK), PE6 (SD)

- [ ] Configurar SAI1_B (DAC1):
  - Master mode, Transmit, Synchronous con SAI1_A
  - Mismo formato
  - Pines: PE2 (MCLK compartido), PE7 (SD), PE8 (SCK), PE9 (FS)

- [ ] Configurar SAI4_B (ADC):
  - Slave mode, Receive
  - BDMA (no DMA regular!)
  - Pines: PC0 (FS), PC1 (SCK), PB11 (SD)

**Verificación sin chips:**
```c
// Generar señal cuadrada en SD de SAI para verificar con LED
// (No es audio real, solo verifica que SAI esté generando clocks)

// Configurar SAI1_A para generar MCLK y clocks
HAL_SAI_Init(&hsai_BlockA1);
HAL_SAI_Transmit(&hsai_BlockA1, dummy_data, 16, 1000);

// Monitorear pin PE2 con multímetro DC - debería medir ~1.65V (señal cuadrada 50%)
```

**Método de verificación indirecta:**
- Conectar LED con resistor a PE2 (MCLK)
- Si parpadea muy rápido (no perceptible a ojo), MCLK está activo
- Usar PWM en otro pin a frecuencia conocida para comparar

### 3.2 Configuración DMA/BDMA (Día 18-19)

**Tareas críticas:**
- [ ] Configurar DMA1 Stream 0 para SAI1_A (DMAMUX1 request 87)
- [ ] Configurar DMA1 Stream 5 para SAI1_B (DMAMUX1 request 88)
- [ ] ⚠️ **CRÍTICO:** Configurar **BDMA Channel 7** para SAI4_B (DMAMUX2 request 16)
  - SAI4 está en dominio D3, SOLO puede usar BDMA
  - Ver [stm32/dma_bdma_config.md](stm32/dma_bdma_config.md)

- [ ] Habilitar modo Circular (ping-pong buffer)
- [ ] Configurar callbacks Half Transfer Complete y Transfer Complete

**Estructura de buffers:**
```c
// Buffers en memoria apropiada (ver memory_map.md)
// SAI1: AXI SRAM (0x24000000) - Rápido, DMA-friendly
// SAI4: SRAM4 (0x38000000) - Dominio D3, accesible por BDMA

#define AUDIO_BUFFER_SIZE 512  // samples por canal
#define NUM_CHANNELS_DAC 8
#define NUM_CHANNELS_ADC 2

// DAC0 buffer (SAI1_A) - TDM8
__attribute__((section(".axi_sram")))
int32_t dac0_buffer[2][AUDIO_BUFFER_SIZE * NUM_CHANNELS_DAC]; // Ping-pong

// DAC1 buffer (SAI1_B) - TDM8
__attribute__((section(".axi_sram")))
int32_t dac1_buffer[2][AUDIO_BUFFER_SIZE * NUM_CHANNELS_DAC];

// ADC buffer (SAI4_B) - I2S stereo
__attribute__((section(".sram4")))
int32_t adc_buffer[2][AUDIO_BUFFER_SIZE * NUM_CHANNELS_ADC];
```

**Verificación sin chips conectados:**
```c
// Llenar buffer de TX con patrón conocido (onda cuadrada digital)
void Fill_Test_Pattern(int32_t *buffer, uint16_t size) {
    for (int i = 0; i < size; i++) {
        buffer[i] = (i % 2) ? 0x007FFFFF : 0xFF800000; // +full / -full scale
    }
}

// Iniciar transmisión DMA
Fill_Test_Pattern(dac0_buffer[0], AUDIO_BUFFER_SIZE * NUM_CHANNELS_DAC);
HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*)dac0_buffer[0],
                     AUDIO_BUFFER_SIZE * NUM_CHANNELS_DAC);

// Callback para monitorear
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
    printf("TX Half Complete\r\n");
    buffer_half_full = 1;
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
    printf("TX Complete\r\n");
    buffer_full = 1;
}

// Si los callbacks se ejecutan periódicamente → DMA funcionando
```

### 3.3 Generación de Audio Sintético (Día 20-21)

**Objetivo:** Generar señales de prueba sin ADC real.

**Tareas:**
- [ ] Implementar generador de onda senoidal
- [ ] Implementar generador de onda cuadrada
- [ ] Implementar generador de ruido blanco
- [ ] Implementar sweep (barrido de frecuencias)

**Código:**
```c
#include <math.h>

// Generador de seno (440 Hz @ 48kHz)
void Generate_Sine_Wave(int32_t *buffer, uint16_t size, float freq) {
    const float fs = 48000.0f;
    const float amplitude = 0x007FFFFF; // 24-bit max
    static float phase = 0.0f;

    for (int i = 0; i < size; i++) {
        buffer[i] = (int32_t)(amplitude * sinf(2.0f * M_PI * freq * phase / fs));
        phase += 1.0f;
        if (phase >= fs) phase -= fs;
    }
}

// Simular entrada de ADC
void Simulate_ADC_Input(void) {
    if (buffer_half_full) {
        Generate_Sine_Wave(&adc_buffer[0][0], AUDIO_BUFFER_SIZE * NUM_CHANNELS_ADC, 440.0f);
        buffer_half_full = 0;
    }
    if (buffer_full) {
        Generate_Sine_Wave(&adc_buffer[1][0], AUDIO_BUFFER_SIZE * NUM_CHANNELS_ADC, 440.0f);
        buffer_full = 0;
    }
}
```

### 3.4 Implementación de Beamforming Básico (Día 21)

**Objetivo:** Procesar audio sintético con delays y ganancias.

**Algoritmo:**
```c
// Delay buffers (uno por canal)
#define MAX_DELAY_SAMPLES 16384  // ~341ms @ 48kHz
__attribute__((section(".sram1")))
int32_t delay_buffers[16][MAX_DELAY_SAMPLES];

uint16_t delay_write_index[16] = {0};

// Configuración desde UART
extern Audio_Config_t audio_config;

// Procesamiento en tiempo real
void Process_Beamforming(int32_t *input, int32_t *output, uint16_t size) {
    for (int sample = 0; sample < size; sample++) {
        // Leer muestra de entrada (ADC simulado)
        int32_t input_sample = input[sample];

        // Aplicar a cada canal con delay y ganancia
        for (int ch = 0; ch < 16; ch++) {
            // Escribir en delay buffer
            delay_buffers[ch][delay_write_index[ch]] = input_sample;

            // Leer muestra retardada
            uint16_t read_index = (delay_write_index[ch] + MAX_DELAY_SAMPLES
                                   - audio_config.channel_delays[ch])
                                  % MAX_DELAY_SAMPLES;
            int32_t delayed_sample = delay_buffers[ch][read_index];

            // Aplicar ganancia
            float gain = audio_config.master_gain * audio_config.channel_gains[ch];
            int32_t processed = (int32_t)(delayed_sample * gain);

            // Aplicar mute
            if (audio_config.channel_mutes[ch]) {
                processed = 0;
            }

            // Escribir a buffer de salida (interleaved para TDM)
            output[sample * 8 + (ch % 8)] = processed;

            // Incrementar índice
            delay_write_index[ch] = (delay_write_index[ch] + 1) % MAX_DELAY_SAMPLES;
        }
    }
}

// Integrar en callbacks DMA
void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
    // Procesar primera mitad
    Process_Beamforming(&adc_buffer[0][0], &dac0_buffer[0][0], AUDIO_BUFFER_SIZE);
    // Separar canales 8-15 a DAC1
    // ... (copiar canales correspondientes a dac1_buffer)
}
```

**Verificación:**
```c
// Test: Enviar comando via UART para cambiar delay del canal 0
// Verificar en debugger que delay_buffers[0] refleja el cambio
// Usar printf para imprimir valores de output buffer cada N muestras

// Test de ganancia
audio_config.master_gain = 0.5f;  // Amplitud debería reducirse a la mitad
audio_config.channel_gains[0] = 0.0f;  // Canal 0 debería silenciarse
```

---

## ✅ Fase 4: Validación de Hardware PCB (Semana 4)

**Objetivo:** Revisar diseño PCB con ojo crítico y preparar para fabricación.

### 4.1 Checklist de Diseño (Día 22-23)

**Tareas:**
- [ ] Revisar netlist DS.net vs documentación
- [ ] Verificar footprints de componentes:
  - STM32H743VIT6 (LQFP-100)
  - PCM1690 (TSSOP-28)
  - PCM1802 (SOIC-8 o TSSOP-8)
  - ESP32 header
- [ ] Confirmar valores de componentes pasivos:
  - Resistores serie: R17, R33 (33Ω - SAI)
  - Resistores serie: R220 (220Ω - UART)
  - Pull-ups I2C: 5.1k a 3V3
  - Decoupling caps: 100nF + 10µF por chip
- [ ] Verificar alimentación:
  - 5V rail para audio analógico
  - 3.3V rail para digital
  - Ferrite bead entre 5V_analog y 5V_digital
- [ ] Layout crítico:
  - ✅ MCLK (PE2) → length matching ±0.1mm
  - ✅ Ground plane sólido bajo SAI
  - ✅ Separación analógico/digital
  - ✅ Decoupling caps cerca de pines de alimentación

**Herramientas:**
- KiCAD ERC (Electrical Rules Check)
- KiCAD DRC (Design Rules Check)
- Revisar 3D view para clearances mecánicas

### 4.2 Design Review con Checklist RM0433 (Día 24)

**Revisar contra Reference Manual:**
- [ ] SAI1: Pines correctos según Alternate Function mapping (RM0433 Table 7)
- [ ] SAI4: En dominio D3, requiere BDMA ✅
- [ ] I2C1: Open-drain config, pullups externos ✅
- [ ] UART7: Pines PA8/PA15 verificados
- [ ] Clocks: HSE, PLL configurables para 12.288 MHz ✅

### 4.3 BOM (Bill of Materials) (Día 25)

**Generar BOM completa:**
- [ ] Exportar BOM desde KiCAD
- [ ] Buscar componentes en Digikey/Mouser/LCSC
- [ ] Verificar disponibilidad (lead time)
- [ ] Calcular costo por unidad (10 pcs, 15 pcs)

**Componentes críticos:**
- STM32H743VIT6 (verificar stock - puede haber escasez)
- PCM1690 (TI - verificar obsolescencia)
- PCM1802 (TI)
- Cristal 25 MHz
- Reguladores 5V → 3.3V

### 4.4 Decisión de Fabricación (Día 26-28)

Ver sección [Decisión: Fabricación PCB](#-decisión-fabricación-pcb) abajo.

---

## 📦 Decisión: Fabricación PCB

### Análisis de Opciones de Ensamblaje

#### Opción A: Solo PCB (tú soldás todo)
**Costo estimado (10 pcs):**
- PCB: USD 50-80 (PCBWay, 4 layers, 100×100mm)
- Componentes: USD 300-400 (Digikey)
- **Total: ~USD 400**

**Pros:**
- Menor costo inicial
- Control total del proceso
- Aprendes habilidades de soldadura SMD

**Contras:**
- Tiempo: ~2-3 horas por placa (20-30 horas total)
- Riesgo de errores (puentes, componentes dañados por calor)
- Requiere equipo: estación de soldadura, flux, lupa/microscopio
- Componentes pequeños (0402, 0603) son MUY difíciles de soldar a mano

#### Opción B: PCB + Ensamblaje parcial (SMDs pequeños)
**Costo estimado (10 pcs):**
- PCB + ensamblaje SMD: USD 200-300 extra
- Componentes que ellos instalan: ~40-60 pcs (resistores, caps 0402/0603)
- Componentes que tú soldás: ~15 pcs (ICs grandes, conectores)
- **Total: ~USD 600-700**

**Pros:**
- Ahorro de tiempo masivo: solo 30-45 min por placa
- Componentes críticos (0402) instalados profesionalmente
- Mayor tasa de éxito (menos boards con defectos)

**Contras:**
- Costo inicial más alto

#### Opción C: Ensamblaje completo (todo hecho)
**Costo estimado (10 pcs):**
- PCB + ensamblaje full: USD 400-600 extra
- **Total: ~USD 900-1100**

**Pros:**
- Recibes placas listas para testear
- Cero tiempo de ensamblaje
- QC profesional incluido

**Contras:**
- Costo alto para prototipo
- Menos aprendizaje del proceso

### Recomendación: **Opción B (Ensamblaje Parcial)**

**Justificación:**
1. **Escala de 10-15 placas:** Es suficiente para justificar ensamblaje automático
2. **Ahorro de tiempo:** 20-25 horas → invertir en desarrollo de firmware
3. **Reducción de riesgo:** Componentes 0402/0603 son propensos a errores manuales
4. **Aprendizaje:** Aún soldás ICs grandes (LQFP, TSSOP) que son críticos para tu formación
5. **Costo-beneficio:** USD 200-300 extra es razonable para 10 boards funcionales

**Componentes para que PCBWay ensamble:**
- Resistores 0402, 0603 (~30 pcs)
- Capacitores 0402, 0603 (~20 pcs)
- LEDs SMD
- Cristales (difíciles de alinear)

**Componentes para soldar tú:**
- STM32H743VIT6 (LQFP-100) - Práctica valiosa
- PCM1690 (TSSOP-28) × 2
- PCM1802 (SOIC-8)
- Conectores (headers, microSD)
- Componentes through-hole

### Proceso Recomendado

1. **Antes de fabricar:**
   - [ ] Finalizar Fases 1-3 (firmware básico funcional)
   - [ ] Hacer última revisión de DS.net
   - [ ] Generar Gerbers + BOM + CPL (Component Placement List)

2. **Orden en PCBWay:**
   - Subir Gerbers
   - Seleccionar "PCB Assembly"
   - Especificar qué componentes ensamblar (marcar en BOM)
   - Esperar cotización (2-3 días)
   - Lead time típico: 2-3 semanas

3. **Mientras esperas fabricación (2-3 semanas):**
   - Continuar con desarrollo de firmware
   - Implementar funcionalidades avanzadas
   - Preparar plan de testing con equipamiento real
   - Aprender soldadura de ICs grandes (practicar en board dummy)

4. **Al recibir PCBs:**
   - Inspección visual (verificar ensamblaje SMD)
   - Soldar componentes restantes (ICs, conectores)
   - Test eléctrico básico (continuidad, cortos)
   - Power-on sequence
   - Programar y testear

---

## 🔮 Roadmap Post-Fabricación

### Mes 2: Validación de Hardware
- [ ] Testing con osciloscopio:
  - Verificar MCLK (12.288 MHz)
  - Verificar SAI BCK, LRCK, SD
  - Medir integridad de señal
- [ ] Testing con analizador lógico:
  - Capturar tráfico I2C
  - Verificar protocolo UART
- [ ] Programar PCM1690 via I2C:
  - Configurar modo Slave
  - Configurar atenuación por canal
  - Verificar funcionamiento
- [ ] Testing de ADC PCM1802:
  - Conectar micrófono
  - Capturar muestras
  - Verificar formato I2S

### Mes 3: Integración de Audio
- [ ] Conectar speakers a DAC outputs
- [ ] Implementar beamforming real
- [ ] Calibración de delays
- [ ] Testing acústico
- [ ] Medición de directividad

### Mes 4: Sistema Completo
- [ ] Integración con webserver
- [ ] Testing de múltiples unidades (sistema de 8 DS)
- [ ] Optimización de algoritmos
- [ ] Documentación de usuario

### Año 1: Producto Comercial
- [ ] Diseño de PCB v2 (mejoras basadas en v1)
- [ ] Certificaciones (CE, FCC si exportas)
- [ ] Desarrollo de software de calibración
- [ ] Primeros clientes piloto

---

## 🎓 Formación Profesional en Sistemas Embebidos

### Recursos de Aprendizaje (Mes 1-3)

**Cursos Online Recomendados:**
1. **"Mastering STM32" (Udemy)** - Cobertura completa de periféricos
2. **"Embedded Systems - Shape The World" (edX)** - UT Austin
3. **"Real-Time Embedded Systems" (Coursera)** - University of Colorado
4. **FastBit Embedded Brain Academy** - Cursos especializados STM32

**Libros Esenciales:**
1. **"Mastering STM32" - Carmine Noviello** (enfoque práctico)
2. **"The Definitive Guide to ARM Cortex-M7" - Joseph Yiu** (arquitectura profunda)
3. **"Embedded Systems Design" - Steve Heath** (teoría general)
4. **"Making Embedded Systems" - Elecia White** (mejores prácticas)

**Documentación Técnica (Leer durante el mes):**
1. RM0433 - STM32H7 Reference Manual (las secciones que uses)
2. PM0253 - STM32 Cortex-M7 Programming Manual
3. AN4661 - Getting started with STM32H7 MCUs
4. Datasheets de PCM1690, PCM1802

### Plan de Formación Profesional (6-12 meses)

**Objetivo:** Convertirte en experto de sistemas embebidos de audio profesional en Argentina.

#### Fase 1: Fundamentos Técnicos (Mes 1-3)
- [x] Dominar STM32CubeIDE, HAL, debugging
- [ ] Entender arquitectura ARM Cortex-M7 profundamente
- [ ] Dominar periféricos críticos: DMA, SAI, I2C, SPI, UART
- [ ] Implementar RTOS (FreeRTOS) en proyecto DS
- [ ] Aprender DSP básico (filtros FIR/IIR, FFT)

#### Fase 2: Diseño de Hardware (Mes 4-6)
- [ ] Curso de KiCAD avanzado
- [ ] Aprender diseño de PCB multi-capa
- [ ] Entender impedancia controlada (para high-speed signals)
- [ ] EMI/EMC - Diseño para pasar certificaciones
- [ ] Thermal management en productos embebidos
- [ ] Practicar con proyectos menores (sensores, actuadores)

#### Fase 3: Audio y DSP (Mes 7-9)
- [ ] Curso de DSP (Coursera: Digital Signal Processing - EPFL)
- [ ] Implementar algoritmos de beamforming avanzados
- [ ] Aprender CMSIS-DSP library (optimizada para ARM)
- [ ] Estudiar acústica: directividad, room acoustics
- [ ] Testing con software: REW (Room EQ Wizard), ARTA

#### Fase 4: Producción y Negocio (Mes 10-12)
- [ ] Diseñar para manufacturabilidad (DFM)
- [ ] Entender proceso de certificación en Argentina (ENACOM)
- [ ] Certificaciones internacionales (CE para Europa, FCC para USA)
- [ ] Supply chain: proveedores confiables, negociación de precios
- [ ] Desarrollar marca personal: LinkedIn, blog técnico, GitHub
- [ ] Networking: asistir a conferencias (Embedded World virtual, etc.)

### Estrategia de Negocio

**Nicho:** Sistemas de audio direccional y beamforming para:
- Instalaciones de sonido profesional (teatros, auditorios)
- Museums (audio dirigido a zonas específicas)
- Retail (publicidad direccional)
- Corporate (salas de conferencias con audio zonal)

**Diferenciadores:**
1. **Open architecture:** Permites customización (vs sistemas cerrados de competencia)
2. **Precio:** Sistema local más económico que importados
3. **Soporte local:** Ventaja clave en Argentina vs importar
4. **Escalabilidad:** Sistema modular (1 a 8+ unidades)

**Plan de Go-to-Market (12-18 meses):**
1. **Meses 1-6:** Desarrollo, prototipo, testing
2. **Meses 7-9:** Beta con 2-3 clientes piloto (estudios de grabación, instaladores)
3. **Meses 10-12:** Iteración v2, certificaciones
4. **Meses 13-18:** Lanzamiento comercial:
   - Sitio web profesional
   - Demos en ferias (BAFIM - Buenos Aires Audio Show)
   - Alianzas con integradores AV
   - Presencia en Salta y CABA

**Proyección de Ingresos (conservadora):**
- Año 1: 10 sistemas @ USD 2000 = USD 20k
- Año 2: 30 sistemas @ USD 1800 = USD 54k
- Año 3: 60 sistemas @ USD 1500 = USD 90k

---

## 📝 Notas Finales

**Prioridades del Mes 1:**
1. ✅ Familiarizarse con STM32 y herramientas (Semana 1)
2. ✅ Establecer comunicación ESP32-STM32 robusta (Semana 2)
3. ✅ Implementar sistema de audio virtual sin hardware (Semana 3)
4. ✅ Decidir sobre fabricación y mandar PCB a producción (Semana 4)

**Cómo Aprovechar el Tiempo sin Laboratorio:**
- Usar debugging intensivo (step-through, watch variables)
- Implementar logging detallado por UART
- Crear simulaciones de señales
- Estudiar teoría (RM0433, datasheets, cursos)
- Networking con comunidad online (STM32 forums, Reddit r/embedded)

**Próximos Pasos Después de Este Plan:**
1. Al recibir PCBs: ejecutar testing_procedures_physical.md (a crear)
2. Documentar problemas encontrados en hardware
3. Iterar sobre diseño si es necesario
4. Escalar producción una vez validado

---

**Última Actualización:** 2026-01-24
**Autor:** Bergamí (con asistencia de Claude Code)
**Versión:** 1.0

**Estado del Proyecto:**
- Documentación: ✅ Completa
- Hardware: ⚠️ Esperando fabricación
- Firmware: 🔄 En desarrollo (Fase 1)
- Webserver: ✅ Funcional
- Comunicación ESP32: ⏳ Pendiente (Fase 2)
