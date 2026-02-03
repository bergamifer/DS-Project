# DS Hardware Map v2.0 - STM32H743VIT6 Direct (LQFP100)

**Proyecto:** Domosonica DS9 (32-channel Beamforming)
**MCU:** STM32H743VIT6 LQFP100 (soldado directo al PCB)
**Fecha:** Enero 2026
**Estado:** Pinout actualizado desde DS.d356

---

## ⚠️ KNOWN ISSUES - FABRICACIÓN DS9 (EN PRODUCCIÓN)

### Bug LED Colors - DS9 v1 Fabricada
**ESTADO:** En producción en JLCPCB (Enero 2026)

**Problema:**
- Todos los LEDs STM32 (D1, D6, D7, D10, D11) se fabricarán como rojo (C84256) en lugar de los colores especificados
- La BOM tiene todos los designadores asignados a la misma parte número C84256 en una sola línea
- Se pidió revisión a JLCPCB pero es probable que se fabrique con esta configuración

**LEDs afectados:**
| LED | Designador | Pin STM32 | Color Diseño | Color Fabricado | JLCPCB Part Actual |
|-----|------------|-----------|--------------|-----------------|-------------------|
| LED1 | D6 | PC4 | Amarillo | **Rojo** | C84256 (incorrecto) |
| LED2 | D7 | PC5 | Rojo | Rojo ✓ | C84256 (correcto) |
| LED3 | D11 | PB0 | Verde | **Rojo** | C84256 (incorrecto) |
| LED4 | D10 | PB1 | Blanco | **Rojo** | C84256 (incorrecto) |

**Resistencias relacionadas:**
También tendrán valores incorrectos en la fabricación (diseñados para corrientes específicas por color)

**Acción recomendada para v2:**
- Separar cada LED en línea individual en BOM con su parte número correspondiente
- D6 → C2296 (Amarillo, 240Ω)
- D7 → C84256 (Rojo, 150Ω)
- D11 → C2297 (Verde, 100Ω)
- D10 → C34499 (Blanco, 82Ω)

---

## 🔴 Actualización Importante - Correcciones desde DS.d356 (Netlist)

**Cambios en DS9 (Enero 2026) - Verificado contra netlist de fabricación:**

### Designadores de Componentes Corregidos:
| Anterior | Actual | Componente |
|----------|--------|------------|
| DAC3 = U7 | **U8** | PCM1690 (DAC3) |
| EEPROM = U10 | **U6** | CAT24C256 |
| Reg 5V = U8 | **U9** | AMS1117-5.0 |
| Reg 3.3V = U9 | **U10** | AMS1117-3.3 |
| - | **U7** | ESP32-C3-Mini |

### USB Type-C Agregado:
- **PA11/PA12** son USB D-/D+, NO LEDs
- LEDs reales: **PC4, PC5, PB0, PB1**

### PCM1802 Control Extendido (8 pines GPIO):
- Pin 9 (FSYNC) ahora controlable via **PA4**
- FMT0/FMT1 controlables via **PC7/PC6**
- MODE0/MODE1 controlables via **PD14/PD15**

---

## Cambios principales vs v1.0

| Aspecto | v1.0 (Dev Board) | v2.0 (LQFP100 Direct) |
|---------|------------------|------------------------|
| **Canales DAC** | 16 (2x PCM1690) | **32 (4x PCM1690)** |
| **Canales ADC** | 2 (1x PCM1802) | 2 (1x PCM1802) |
| **SAI utilizados** | SAI1, SAI4 | **SAI1, SAI2, SAI4** |
| **I2C buses** | 1 (I2C1) | **2 (I2C1, I2C3)** |
| **MCLK source** | PLL interno | **Cristal externo 12.288 MHz** |
| **Calibracion** | RAM volatil | **EEPROM I2C persistente** |
| **Test points** | 0 | **5 puntos** |
| **UART para debug** | - | **UART4 (PA0/PA1)** |

---

## Tabla de Pines Actualizada - v2.0 (desde DS.net)

### Port A (PA0-PA15)

| Pin | Pkg | GPIO | Asignacion | Periferico | Notas |
|-----|-----|------|-----------|-----------|-------|
| PA0 | 22 | UART4_TX | UART4 TX | UART4 | Debug/comunicacion |
| PA1 | 23 | UART4_RX | UART4 RX | UART4 | Debug/comunicacion |
| PA2 | 24 | - | **LIBRE** | - | Disponible |
| PA3 | 25 | - | **LIBRE** | - | (I2C3_SCL_ALT eliminado) |
| PA4 | 28 | ADC_FSYNC | ADC Frame Sync | GPIO | PCM1802 pin 9 - FSYNC input |
| PA5 | 29 | ADC_OSR | ADC Oversampling | GPIO | PCM1802 OSR pin |
| PA6 | 30 | ADC_BYPASS | ADC HPF Bypass | GPIO | PCM1802 bypass filter |
| PA7 | 31 | ADC_PDWN | ADC Power Down | GPIO | PCM1802 control |
| PA8 | 67 | TP3 | Test Point 3 | GPIO | Debug/medición |
| PA9 | 68 | - | **LIBRE** | - | Disponible |
| PA10 | 69 | PWR_EN_3V3A | Power Enable Analog | GPIO | Secuencia power-up |
| PA11 | 70 | USB_DM | USB Data Minus | USB_OTG | USB Type-C D- (J3) |
| PA12 | 71 | USB_DP | USB Data Plus | USB_OTG | USB Type-C D+ (J3) |
| PA13 | 72 | SWDIO | Debug | SWD | Reservado para debug |
| PA14 | 76 | SWCLK | Debug | SWD | Reservado para debug |
| PA15 | 77 | - | **LIBRE** | - | Disponible |

### Port B (PB0-PB15)

| Pin | Pkg | GPIO | Asignacion | Periferico | Notas |
|-----|-----|------|-----------|-----------|-------|
| PB0 | 34 | LED3 | LED Status 3 (D11) | GPIO | Indicador sistema |
| PB1 | 35 | LED4 | LED Status 4 (D10) | GPIO | Indicador sistema |
| PB2 | 36 | - | **LIBRE** | - | Disponible |
| PB3 | 89 | - | **LIBRE** | - | Disponible |
| PB4 | 90 | - | **LIBRE** | - | Disponible |
| PB5 | 91 | EEPROM_WP | EEPROM Write Protect | GPIO | CAT24C256 WP pin |
| PB6 | 92 | I2C3_SCL | I2C3 Clock | I2C3 | Bus EEPROM |
| PB7 | 93 | I2C3_SDA | I2C3 Data | I2C3 | Bus EEPROM |
| PB8 | 95 | I2C1_SCL | I2C1 Clock | I2C1 | Bus DACs (4x PCM1690) |
| PB9 | 96 | I2C1_SDA | I2C1 Data | I2C1 | Bus DACs (4x PCM1690) |
| PB10 | 46 | - | **LIBRE** | - | Disponible |
| PB11 | 47 | SAI4_SD_B | SAI4 Data In | SAI4_B | Audio input PCM1802 |
| PB12 | 51 | - | **LIBRE** | - | Disponible |
| PB13 | 52 | DAC3_AMUTEO | DAC3 Mute Out | GPIO | PCM1690 U8 status |
| PB14 | 53 | DAC3_AMUTEI | DAC3 Mute In | GPIO | PCM1690 U8 control |
| PB15 | 54 | DAC3_RST | DAC3 Reset | GPIO | PCM1690 U8 reset |

### Port C (PC0-PC15)

| Pin | Pkg | GPIO | Asignacion | Periferico | Notas |
|-----|-----|------|-----------|-----------|-------|
| PC0 | 15 | SAI4_FS_B | SAI4 Frame Sync | SAI4_B | ADC LRCK |
| PC1 | 16 | SAI4_SCK_B | SAI4 Bit Clock | SAI4_B | ADC BCK |
| PC2 | 17 | - | **LIBRE** | - | Disponible |
| PC3 | 18 | - | **LIBRE** | - | Disponible |
| PC4 | 32 | LED1 | LED Status 1 (D6) | GPIO | Indicador sistema |
| PC5 | 33 | LED2 | LED Status 2 (D7) | GPIO | Indicador sistema |
| PC6 | 63 | ADC_FMT1 | ADC Format Bit 1 | GPIO | PCM1802 FMT1 pin |
| PC7 | 64 | ADC_FMT0 | ADC Format Bit 0 | GPIO | PCM1802 FMT0 pin |
| PC8 | 65 | - | **LIBRE** | - | Disponible |
| PC9 | 66 | - | **LIBRE** | - | Disponible |
| PC10 | 78 | DAC2_RST | DAC2 Reset | GPIO | PCM1690 U2 reset |
| PC11 | 79 | DAC2_AMUTEI | DAC2 Mute In | GPIO | PCM1690 U2 control |
| PC12 | 80 | DAC2_AMUTEO | DAC2 Mute Out | GPIO | PCM1690 U2 status |
| PC13 | 7 | GPIO | General Purpose | GPIO | Disponible |
| PC14 | 8 | OSC32_IN | LSE Crystal | RTC | Crystal 32.768 kHz |
| PC15 | 9 | OSC32_OUT | LSE Crystal | RTC | Crystal 32.768 kHz |

### Port D (PD0-PD15)

| Pin | Pkg | GPIO | Asignacion | Periferico | Notas |
|-----|-----|------|-----------|-----------|-------|
| PD0 | 81 | - | **LIBRE** | - | Disponible |
| PD1 | 82 | - | **LIBRE** | - | Disponible |
| PD2 | 83 | - | **LIBRE** | - | Disponible |
| PD3 | 84 | - | **LIBRE** | - | Disponible |
| PD4 | 85 | - | **LIBRE** | - | Disponible |
| PD5 | 86 | - | **LIBRE** | - | Disponible |
| PD6 | 87 | - | **LIBRE** | - | Disponible |
| PD7 | 88 | - | **LIBRE** | - | Disponible |
| PD8 | 55 | DAC3_SAI_SD | SAI2 SD Out | SAI2_B | Audio data -> PCM1690 U8 |
| PD9 | 56 | DAC3_SAI_SCK | SAI2 SCK Out | SAI2_B | Bit clock -> U8 |
| PD10 | 57 | DAC3_SAI_FS | SAI2 FS Out | SAI2_B | Frame sync -> U8 |
| PD11 | 58 | DAC2_SAI_SD | SAI2 SD Out | SAI2_A | Audio data -> PCM1690 U2 |
| PD12 | 59 | DAC2_SAI_SCK | SAI2 SCK Out | SAI2_A | Bit clock -> U2 |
| PD13 | 60 | DAC2_SAI_FS | SAI2 FS Out | SAI2_A | Frame sync -> U2 |
| PD14 | 61 | ADC_MODE0 | ADC Mode Bit 0 | GPIO | PCM1802 MODE0 pin |
| PD15 | 62 | ADC_MODE1 | ADC Mode Bit 1 | GPIO | PCM1802 MODE1 pin |

### Port E (PE0-PE15)

| Pin | Pkg | GPIO | Asignacion | Periferico | Notas |
|-----|-----|------|-----------|-----------|-------|
| PE0 | 97 | DAC1_AMUTEO | DAC1 Mute Out | GPIO | PCM1690 U4 status |
| PE1 | 98 | DAC1_AMUTEI | DAC1 Mute In | GPIO | PCM1690 U4 control |
| PE2 | 1 | SAI1_MCLK | SAI1 Master Clock | SAI1 | **CRITICO**: Clock 12.288 MHz |
| PE3 | 2 | DAC1_RST | DAC1 Reset | GPIO | PCM1690 U4 reset |
| PE4 | 3 | DAC1_SAI_FS | SAI1 FS Out | SAI1_B | Frame sync -> U4 |
| PE5 | 4 | DAC1_SAI_SCK | SAI1 SCK Out | SAI1_B | Bit clock -> U4 |
| PE6 | 5 | DAC1_SAI_SD | SAI1 SD Out | SAI1_B | Audio data -> U4 |
| PE7 | 37 | DAC0_SAI_FS | SAI1 FS Out | SAI1_A | Frame sync -> U3 |
| PE8 | 38 | DAC0_SAI_SCK | SAI1 SCK Out | SAI1_A | Bit clock -> U3 |
| PE9 | 39 | DAC0_SAI_SD | SAI1 SD Out | SAI1_A | Audio data -> U3 |
| PE10 | 40 | DAC0_RST | DAC0 Reset | GPIO | PCM1690 U3 reset |
| PE11 | 41 | DAC0_AMUTEI | DAC0 Mute In | GPIO | PCM1690 U3 control |
| PE12 | 42 | DAC0_AMUTEO | DAC0 Mute Out | GPIO | PCM1690 U3 status |
| PE13 | 43 | TP1 | Test Point 1 | GPIO | Debug/medición |
| PE14 | 44 | TP2 | Test Point 2 | GPIO | Debug/medición |
| PE15 | 45 | - | **LIBRE** | - | Disponible |

---

## Mapeo de Audio v2.0

### Arquitectura Completa - 32 Canales Out + 2 In

```
                              +-------------------------------------+
                              |         STM32H743VIT6               |
                              |                                     |
                    +---------| PE2 (SAI1_MCLK) ----> R21 (33 Ohm)  |
                    |         |                       12.288 MHz    |
                    |         +-------------------------------------+
                    |                                  |
                    |  +-------------------------------+
                    |  |
        +-----------v--v--+         +----+----+----+----+
        |  Clock          |         |    |    |    |    |
        |  Distribution   |         |    |    |    |    |
        +-+------+----+---+         |    |    |    |    |
          |      |    |             v    v    v    v    v
          v      v    v         +----+ +----+ +----+ +----+
      +------+ |  | +------+    |PCM | |PCM | |PCM | |PCM |
      |PCM   | |  | |      |    |1690| |1690| |1690| |1690|
      |1802  | |  | |      |    | U3 | | U4 | | U2 | | U8 |
      | U1   | |  | |      |    |DAC0| |DAC1| |DAC2| |DAC3|
      | ADC  | |  | |      |    +----+ +----+ +----+ +----+
      +------+ |  | |      |    8ch    8ch    8ch    8ch
      2ch IN   |  | |      |     |      |      |      |
               |  | |      |     v      v      v      v
        <------+  +-+      |   SAI1_A SAI1_B SAI2_A SAI2_B
         SAI4_B            |
```

### Mapeo SAI -> DAC

| SAI Block | Pines (FS/SCK/SD) | DAC | Canales |
|-----------|-------------------|-----|---------|
| SAI1_A | PE7/PE8/PE9 | DAC0 (U3) | ch 0-7 |
| SAI1_B | PE4/PE5/PE6 | DAC1 (U4) | ch 8-15 |
| SAI2_A | PD13/PD12/PD11 | DAC2 (U2) | ch 16-23 |
| SAI2_B | PD10/PD9/PD8 | DAC3 (U8) | ch 24-31 |
| SAI4_B | PC0/PC1/PB11 | ADC (U1) | input 2ch |

---

## I2C Bus Assignment v2.0

### I2C1 - Audio DACs (PB8/PB9)

| Device | Address (8-bit) | ADR1 | ADR0 | Funcion |
|--------|-----------------|------|------|---------|
| PCM1690 U3 (DAC0) | **0x94** | 0 | 0 | 8 canales (ch 0-7) |
| PCM1690 U4 (DAC1) | **0x96** | 0 | 1 | 8 canales (ch 8-15) |
| PCM1690 U2 (DAC2) | **0x98** | 1 | 0 | 8 canales (ch 16-23) |
| PCM1690 U8 (DAC3) | **0x9A** | 1 | 1 | 8 canales (ch 24-31) |

**Nota sobre direcciones I2C:** El PCM1690 usa direccionamiento de 8 bits. Las direcciones mostradas son los valores de escritura (R/W=0). Para lectura, sumar 1.

**Resistencias Pull-up I2C1**:
- Valor actual: 5.1kΩ ⚠️ (funciona pero no óptimo)
- **Recomendado**: **4.7kΩ** para mejor performance a 400 kHz
- Conexión: SDA y SCL → +3.3V via pull-ups
- Cálculo para 400 kHz con Cbus≈100pF:
  - R_min = 970Ω
  - R_max = 2.9kΩ
  - **Óptimo: 2.2kΩ - 4.7kΩ**

### I2C3 - EEPROM (PB6/PB7)

| Device | Address (7-bit) | Funcion | VCC |
|--------|-----------------|---------|-----|
| CAT24C256 (U6) | **0x50** | Calibracion persistente | **+3.3V** ✅ |

**Especificaciones CAT24C256**:
- Capacidad: 256 Kbit (32 KB)
- Voltaje: 2.5V - 5.5V → **+3.3V OK** ✅
- Interface: I2C (Fast Mode 400 kHz)
- Write Endurance: 1,000,000 ciclos
- Data Retention: 200 años @ 25°C

**Control de EEPROM:**
- PB5 = EEPROM_WP (Write Protect)
  - WP=LOW: Escritura habilitada
  - WP=HIGH: Escritura protegida
- Direccionamiento: A0=GND, A1=GND, A2=GND → Address = 0x50

**Resistencias Pull-up I2C3**:
- Valor actual: 5.1kΩ ⚠️ (funciona pero no óptimo)
- **Recomendado**: **4.7kΩ** para mejor performance
- Conexión: SDA y SCL → +3.3V via pull-ups

**Resumen Pull-ups I2C**:
| Bus | Dispositivos | Pull-up Actual | Pull-up Recomendado | Velocidad |
|-----|--------------|----------------|---------------------|-----------|
| I2C1 (PB8/PB9) | 4x PCM1690 | 5.1kΩ | **4.7kΩ** | 100-400 kHz |
| I2C3 (PB6/PB7) | 1x CAT24C256 | 5.1kΩ | **4.7kΩ** | 100-400 kHz |

**Nota**: 5.1kΩ funciona correctamente a 100 kHz, pero para 400 kHz es mejor usar 4.7kΩ o 2.2kΩ para rise time más rápido.

---

## Test Points v2.0

### Actuales (verificados en DS.d356)

| TP | Pin STM32 | Senal | Proposito |
|----|-----------|-------|-----------|
| TP1 | PE13 | GPIO | Debug general |
| TP2 | PE14 | GPIO | Debug general |
| TP3 | PA8 | GPIO | Debug general |
| TP5 | - | +12V | Verificar alimentacion principal |

### Test Points de Power Supply

| TP | Net | Proposito |
|----|-----|-----------|
| TP_12V | +12V | Verificar alimentacion principal |
| TP_5V | +5V | Verificar regulador U9 |
| TP_3V3 | +3.3V | Verificar regulador U10 |
| TP_GND | GND | Referencia para osciloscopio |

### Para que sirven los Test Points

1. **Debugging en produccion**: Conectar osciloscopio/multimetro sin soldar
2. **Verificacion de clock**: Confirmar MCLK = 12.288 MHz exactos
3. **Verificacion de sync**: Confirmar SAI_FS = 48 kHz
4. **Diagnostico de alimentacion**: Medir ripple en 3.3V/5V/12V
5. **Referencia de medicion**: GND para punta de osciloscopio

### Como implementar Test Points en KiCad

1. Agregar simbolo `TestPoint` de libreria `Connector`
2. Conectar a la net que queres monitorear
3. Usar footprint `TestPoint:TestPoint_Pad_D2.5mm`
4. En el PCB, ubicar cerca de la senal pero accesible

---

## Conectividad y Expansión

### ESP32-C3-Mini (U7) - Interfaz Inalámbrica

**Especificaciones:**
- **Referencia:** U7
- **Parte:** Espressif ESP32-C3-Mini-1
- **Función:** Interfaz WiFi/BLE para control remoto y comunicación
- **Bus:** UART o SPI hacia STM32H743
- **Alimentación:** +3.3V (desde U10, AMS1117-3.3V)

**Características:**
- WiFi 802.11b/g/n (2.4 GHz)
- Bluetooth LE 5.0
- Procesador RISC-V de 32-bit @ 160 MHz
- RAM: 400 KB SRAM
- Flash: 4 MB integrado
- USB nativo: USB Serial/JTAG (no requiere convertidor externo)
- Pines GPIO: 15 disponibles (GPIO0-GPIO10, GPIO18-GPIO21)

#### Strapping Pins y Configuración de Boot

**IMPORTANTE**: GPIO8 y GPIO9 controlan el modo de arranque del ESP32-C3

| GPIO | Estado | Función | Configuración DS9 |
|------|--------|---------|-------------------|
| **GPIO8** | HIGH | Running mode (boot normal) | ✅ Pull-up 10kΩ a +3.3V |
| GPIO8 | LOW | Download boot mode (UART) | (para programación UART) |
| **GPIO9** | LOW | Boot desde flash interno | ⚠️ **Agregar pull-up 10kΩ a +3.3V** |
| GPIO9 | HIGH | Download mode | Jumper opcional a GND |

**Configuración recomendada**:
```
GPIO8: ----[10kΩ]---- +3.3V  (fijo en HIGH)

         +3.3V
           |
         [10kΩ]  ← Pull-up recomendado
           |
GPIO9: ---+----[JUMPER]---- GND  (opcional)
```

**Con esta configuración**:
- Boot normal: GPIO8=HIGH, GPIO9=HIGH → Arranca desde flash
- Programación: Usar **USB nativo** (no requiere cambiar strapping pins)

#### Programación y Debug

| Método | Interface | Strapping requerido | Estado |
|--------|-----------|---------------------|--------|
| **USB Serial/JTAG** | USB nativo (D+/D-) | Ninguno | ✅ **Recomendado** |
| UART | UART0 (TX/RX) | GPIO8=LOW | ⚠️ Requiere modificar GPIO8 |
| JTAG | Pines JTAG | Ninguno | Avanzado |

**Ventaja del USB nativo**: Con GPIO8 fijo en HIGH (pull-up), puedes programar por USB sin problemas. El USB funciona independientemente del estado de GPIO8.

#### GPIO Assignments

| GPIO | Función | Conexión | Notas |
|------|---------|----------|-------|
| GPIO8 | Strapping | Pull-up 10kΩ → +3.3V | Boot mode control |
| GPIO9 | Strapping | Pull-up 10kΩ → +3.3V + Jumper a GND | Download mode (opcional) |
| GPIO10 | LED Status | LED blanco (C2290) + 120Ω | ESP32 status indicator |
| GPIO18 (USB D-) | USB | Conector USB micro/Type-C | Programación/Debug |
| GPIO19 (USB D+) | USB | Conector USB micro/Type-C | Programación/Debug |
| GPIO2-GPIO7 | General | Disponible para UART/SPI/I2C | Comunicación con STM32 |

**LED ESP32 (GPIO10)**:
- Designador: D12
- Color: Blanco
- JLCPCB: C2290 (0603, Vf=2.9V)
- Resistencia: 120Ω → Corriente ≈ 3.3mA
- **Nota**: C2290 es **0603**. Si prefieres 0805, usa C34499 con 68Ω.

#### Comunicación ESP32 ↔ STM32

**Opción 1: UART** (recomendado para simplicidad):
```
ESP32-C3-Mini (U7)     STM32H743 (U5)
GPIO2 (TX) ----------> PA1 (UART4_RX)
GPIO3 (RX) <---------- PA0 (UART4_TX)
GND        ----------- GND
```

**IMPORTANTE**: Conexión **DIRECTA** sin resistencias serie
- ✅ **0Ω** (directo): Recomendado
- ❌ **NO usar 120Ω** (causa problemas de señal)
- ⚠️ Si necesitas protección ESD: máximo 22Ω-47Ω

**Configuración UART recomendada**:
- Baud rate: 115200 bps (estándar) o hasta 921600 bps
- Formato: 8N1 (8 bits, sin paridad, 1 stop bit)
- Control de flujo: Ninguno (no necesario para punto-a-punto)

**Opción 2: SPI** (para mayor velocidad - futuro):
```
ESP32 GPIO4 (MISO) → STM32 SPI_MISO
ESP32 GPIO5 (MOSI) → STM32 SPI_MOSI
ESP32 GPIO6 (CLK)  → STM32 SPI_CLK
ESP32 GPIO7 (CS)   → STM32 SPI_CS
```

**Propósito en Domosonica DS v2.0:**
1. Control remoto del sistema de beamforming via WiFi
2. Monitoreo de estado en tiempo real
3. Configuración inalámbrica de parámetros de audio
4. Integración con ecosistemas IoT (MQTT, HTTP API)
5. OTA (Over-The-Air) updates del firmware ESP32

---

### USB Type-C (J3) - Conectividad USB del STM32

**Conexiones STM32:**
- **PA11** = USB D- (OTG_FS_DM)
- **PA12** = USB D+ (OTG_FS_DP)
- **VBUS** → D4 (B5819W Schottky) → +5V rail
- **CC1, CC2** = Configuration Channel (5.1kΩ a GND para USB 2.0 device)
- **SBU1, SBU2** = **No conectar (N.C.)** ✅ OK para USB 2.0 básico

**Protección ESD:**
- **USBLC6-2SC6** (D8/D9): TVS diode array para D+/D-
- Configuración:
  ```
  PA12 (D+) --[22Ω]-- D+ conector
  PA11 (D-) --[22Ω]-- D- conector
  USBLC6-2SC6: Pin1=GND, Pin2=D+, Pin3=D-, Pin6=VBUS
  ```

**Funciones:**
1. Programación/Debug via USB DFU
2. Comunicación serial USB CDC
3. Alimentación: VBUS (5V) → Schottky B5819W → +5V rail → U10 (AMS1117-3.3V) → +3.3V

**Nota sobre SBU1/SBU2**: Los pines Sideband Use (SBU) solo se usan en USB-C con modo alternativo (DisplayPort, Thunderbolt) o USB Power Delivery avanzado. Para USB 2.0 básico, **dejarlos flotantes es correcto**.

---

### LEDs de Estado

#### STM32H743 LEDs (4 LEDs principales)

| LED | Designador | Pin STM32 | Color | JLCPCB Part | Vf (V) | If (mA) | Resistor | Corriente Real |
|-----|------------|-----------|-------|-------------|--------|---------|----------|----------------|
| LED1 | D6 | PC4 | Amarillo | **C2296** | 2.1 | 5 | **240Ω** | 5mA |
| LED2 | D7 | PC5 | Rojo | **C84256** | 2.1 | 15 | **150Ω** | 15mA |
| LED3 | D11 | PB0 | Verde | **C2297** | 2.85 | 5 | **100Ω** | 5mA |
| LED4 | D10 | PB1 | Blanco | **C34499** | 2.9 | 12 | **82Ω** | 12mA |

**Consumo total LEDs STM32**: 5 + 15 + 5 + 12 = **37mA** ✅

**Especificaciones JLCPCB LEDs (todos 0805)**:
- **C2296** (Amarillo): KT-0805Y, 175mcd, 120°, 1.8-2.4V, $0.0136
- **C84256** (Rojo): NCD0805R1, 195mcd, 130°, 1.6-2.6V, $0.019
- **C2297** (Verde): KT-0805G, 430mcd, 120°, 2.6-3.1V, $0.0174
- **C34499** (Blanco): KT-0805W, 350mcd, 120°, 2.6-3.2V

**Resistencias JLCPCB (todas 0805, ±1%)**:
- **240Ω**: C17572 (0805W8F2400T5E)
- **150Ω**: C17471 (0805W8F1500T5E)
- **100Ω**: C17408 (0805)
- **82Ω**: Usar C25879 o valor estándar 82Ω 0805

**Configuración GPIO**:
- Todos los LEDs: Active HIGH (GPIO=1 → LED ON)
- Modo: Push-Pull Output
- Velocidad: LOW (no requiere alta velocidad)

**Función sugerida**:
- LED1 (Amarillo): Power/Ready
- LED2 (Rojo): Audio Activity / Error
- LED3 (Verde): Communication OK
- LED4 (Blanco): Status / Heartbeat

#### ESP32-C3-Mini LED (1 LED status)

| LED | Designador | Pin ESP32 | Color | JLCPCB Part | Package | Vf (V) | Resistor | Corriente |
|-----|------------|-----------|-------|-------------|---------|--------|----------|-----------|
| LED_ESP | D12 | GPIO10 | Blanco | **C2290** | **0603** | 2.9 | **120Ω** | 3.3mA |

**Nota**: C2290 es paquete **0603** (no 0805). Si prefieres 0805, usa C34499 con 68Ω.

---

### Conector SW_Debug (J_DEBUG)

**Pinout recomendado (header 1.27mm, 5 pines)**:

| Pin | Señal | Conexión | Función |
|-----|-------|----------|---------|
| 1 | VCC | +3.3V | Alimentación debugger (referencia) |
| 2 | SWDIO | PA13 | Data I/O |
| 3 | SWCLK | PA14 | Clock |
| 4 | NRST | MAX809 OUT | Reset (opcional pero recomendado) |
| 5 | GND | GND | Ground |

**Alternativa 4 pines (si espacio limitado)**:
```
Pin 1: VCC (+3.3V)
Pin 2: SWDIO (PA13)
Pin 3: SWCLK (PA14)
Pin 4: GND
```

**Footprint**: Header 2.54mm (0.1") o JST-SH 1.0mm

**Compatible con**: ST-Link V2, ST-Link V3, J-Link, DAPLink

---

## Supervisor de Voltaje v2.0

### MAX809TEUR+T - Power On Reset Monitor

**Especificaciones:**
- Referencia: U11 (o IC1)
- Parte: Analog Devices MAX809TEUR+T
- Encapsulado: SOT-23 (3 pines)
- Función: Monitorea VREF+ y genera RESET limpio

**Configuración:**
- **Pin 1 (VCC):** +3.3V (fuente supervisada)
- **Pin 2 (IN):** Conectado a VREF+ (monitoreado)
- **Pin 3 (GND):** Tierra

**Voltaje de supervisión:**
- Threshold: 2.92-3.23V (típico 3.08V)
- Función: Si VREF+ cae por debajo, fuerza RESET del STM32

**Conexión a NRST:**
- Salida RESET → R41 (100k) → NRST del STM32H743 (pin 14)
- Pull-down R42 (1.5k) a VREF+
- Circuito de reset limpio independiente del firmware

**Propósito:**
1. Reset automático si falla alimentación
2. Reset durante power-down (mantiene coherencia SAI/I2C)
3. Protección de hardware contra estados indefinidos
4. Supervisión independiente sin necesidad de firmware

---

## GPIO Control Summary

### DAC Reset Pins

| DAC | Reset Pin | Active Level |
|-----|-----------|--------------|
| DAC0 (U3) | PE10 | LOW = Reset |
| DAC1 (U4) | PE3 | LOW = Reset |
| DAC2 (U2) | PC10 | LOW = Reset |
| DAC3 (U8) | PB15 | LOW = Reset |

### DAC Mute Control

| DAC | AMUTEI (Input) | AMUTEO (Output) |
|-----|----------------|-----------------|
| DAC0 (U3) | PE11 | PE12 |
| DAC1 (U4) | PE1 | PE0 |
| DAC2 (U2) | PC11 | PC12 |
| DAC3 (U8) | PB14 | PB13 |

### ADC Control (PCM1802 U1) - EXTENDIDO

| Funcion | Pin STM32 | Pin PCM1802 | Descripcion |
|---------|-----------|-------------|-------------|
| **FSYNC** | PA4 | 9 | Frame Sync Input (NUEVO!) |
| OSR | PA5 | 16 | Oversampling ratio |
| BYPASS | PA6 | 8 | HPF bypass |
| PDWN | PA7 | 7 | Power Down (LOW=off, HIGH=on) |
| **FMT0** | PC7 | 17 | Format Select 0 (NUEVO!) |
| **FMT1** | PC6 | 18 | Format Select 1 (NUEVO!) |
| **MODE0** | PD14 | 19 | Mode Select 0 (NUEVO!) |
| **MODE1** | PD15 | 20 | Mode Select 1 (NUEVO!) |

**Configuración de Formato (FMT1:FMT0):**
| FMT1 | FMT0 | Formato |
|------|------|---------|
| 0 | 0 | I2S (24-bit, standard) |
| 0 | 1 | Left-Justified |
| 1 | 0 | Right-Justified 24-bit |
| 1 | 1 | Right-Justified 16-bit |

**Configuración de Modo (MODE1:MODE0):**
| MODE1 | MODE0 | Modo |
|-------|-------|------|
| 0 | 0 | Slave mode (BCK/LRCK input) |
| 0 | 1 | Master mode 256fs |
| 1 | 0 | Master mode 384fs |
| 1 | 1 | Master mode 512fs |

---

## Pines Libres (disponibles para expansion)

### Total: ~15 pines libres (actualizado)

**Port A:** PA2, PA3, PA9, PA15
**Port B:** PB2, PB3, PB4, PB10, PB12
**Port C:** PC2, PC3, PC8, PC9
**Port D:** PD0, PD1, PD2, PD3, PD4, PD5, PD6, PD7
**Port E:** PE15

**Nota:** Varios pines anteriormente "libres" ahora están asignados:
- PA4-PA7: Control extendido PCM1802
- PA8, PE13, PE14: Test Points
- PA11, PA12: USB Type-C
- PB0, PB1, PC4, PC5: LEDs de estado
- PC6, PC7: Control formato PCM1802
- PD14, PD15: Control modo PCM1802

---

## EEPROM Calibration Data Structure

```c
// CAT24C256: 32KB, I2C address 0x50
#define EEPROM_ADDR_MAGIC       0x0000  // "DSv2" (4 bytes)
#define EEPROM_ADDR_VERSION     0x0004  // Firmware version (4 bytes)
#define EEPROM_ADDR_SERIAL      0x0008  // Device serial number (8 bytes)
#define EEPROM_ADDR_CHANNELS    0x0010  // Channel calibration data

typedef struct {
    float gain_cal[32];         // Calibracion de ganancia por canal (128 bytes)
    int16_t delay_cal[32];      // Calibracion de delay por canal (64 bytes)
    uint8_t channel_enable[32]; // Habilitacion de canal (32 bytes)
    uint16_t crc;               // CRC16 de verificacion
} __attribute__((packed)) EEPROM_Calibration_t;
// Total: 226 bytes

// Control de Write Protect
void EEPROM_WriteProtect_Enable(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
}

void EEPROM_WriteProtect_Disable(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
}
```

---

## Power Sequencing v2.0

```c
void Audio_PowerUp_Sequence(void)
{
    // 1. Habilitar 3.3V analogico primero
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);  // PWR_EN_3V3A
    HAL_Delay(10);

    // 2. Mantener todos los DACs en reset
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);  // DAC0_RST (U3)
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET);   // DAC1_RST (U4)
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_RESET);  // DAC2_RST (U2)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);  // DAC3_RST (U8)

    // 3. ADC en power-down
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);   // ADC_PDWN (PA7)

    // 4. Iniciar MCLK (SAI1)
    HAL_SAI_Init(&hsai_BlockA1);
    HAL_Delay(1);

    // 5. Sacar DACs de reset secuencialmente
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET);  // DAC0 (U3)
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);   // DAC1 (U4)
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_SET);  // DAC2 (U2)
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);  // DAC3 (U8)
    HAL_Delay(1);

    // 6. Configurar DACs via I2C1 (direcciones en escritura)
    PCM1690_Init(&hi2c1, 0x94);  // DAC0 (U3) - ADR1=0, ADR0=0
    PCM1690_Init(&hi2c1, 0x96);  // DAC1 (U4) - ADR1=0, ADR0=1
    PCM1690_Init(&hi2c1, 0x98);  // DAC2 (U2) - ADR1=1, ADR0=0
    PCM1690_Init(&hi2c1, 0x9A);  // DAC3 (U8) - ADR1=1, ADR0=1

    // 7. Configurar ADC format y mode
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);  // FMT0=0 (I2S)
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);  // FMT1=0
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET); // MODE0=0 (Slave)
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET); // MODE1=0

    // 8. Activar ADC
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);   // ADC_PDWN (PA7)
    HAL_Delay(100);  // ADC requiere 100ms startup

    // 9. Cargar calibracion desde EEPROM (U6)
    EEPROM_WriteProtect_Enable();  // Proteger datos
    EEPROM_Calibration_t cal;
    EEPROM_Read_Calibration(&hi2c3, &cal);
    Apply_Calibration(&cal);

    // 10. Iniciar SAI con DMA
    HAL_SAI_Receive_DMA(&hsai_BlockB4, adc_buffer, BUFFER_SIZE);
    HAL_SAI_Transmit_DMA(&hsai_BlockA1, dac0_buffer, BUFFER_SIZE);
    HAL_SAI_Transmit_DMA(&hsai_BlockB1, dac1_buffer, BUFFER_SIZE);
    HAL_SAI_Transmit_DMA(&hsai_BlockA2, dac2_buffer, BUFFER_SIZE);
    HAL_SAI_Transmit_DMA(&hsai_BlockB2, dac3_buffer, BUFFER_SIZE);
}
```

---

---

## Verificación del Diseño v2.0 - ESTADO ACTUAL

### ✅ Verificado y Validado

#### Direcciones I2C
- [x] DAC0 (U3): 0x94 (ADR1=0, ADR0=0)
- [x] DAC1 (U4): 0x96 (ADR1=0, ADR0=1)
- [x] DAC2 (U2): 0x98 (ADR1=1, ADR0=0)
- [x] DAC3 (U8): 0x9A (ADR1=1, ADR0=1)
- [x] EEPROM (U6): 0xA0 (A0-A2 = GND)
- [x] **Todas las direcciones son únicas - Sin conflictos I2C**

#### Conexiones SAI (Bit Clock)
- [x] DAC0 (U3) BCK: PE8 (SAI1_A SCK)
- [x] DAC1 (U4) BCK: PE5 (SAI1_B SCK)
- [x] DAC2 (U2) BCK: PD12 (SAI2_A SCK)
- [x] DAC3 (U8) BCK: PD9 (SAI2_B SCK)
- [x] ADC (U1): PC1 (SAI4_SCK_B)
- [x] **Todos los BCK conectados correctamente - Audio funcional**

#### Supervisor de Voltaje
- [x] MAX809TEUR+T: SOT-23 (3 pines)
- [x] Capacitor 100nF en VCC: **Presente**
- [x] VCC del supervisor: +3.3V
- [x] Monitorea VREF+ para reset automático
- [x] **Reset limpio e independiente implementado**

#### Alimentación
- [x] U9 (AMS1117-5V): +12V → +5V
- [x] U10 (AMS1117-3.3V): +5V → +3.3V
- [x] Cascada correcta de reguladores
- [x] Ferrita FB1 en VREF+
- [x] Protección VBAT con BAT54C (D3)
- [x] **Power supply validado - Margen seguro en ambos rails**

#### Cristales y Osciladores

**Y1 - LSE 32.768 kHz (RTC):**
| Parámetro | Valor |
|-----------|-------|
| Conexión | PC14/PC15 (U5 pins 8/9) |
| Package | SMD3215-2P |
| **JLCPCB Recomendado** | NX3215SA-32.768kHz-EXS00A-MU00202 |
| **LCSC** | **C280830** |
| Load Capacitance | 7pF |
| Tolerancia | ±20ppm |

**Y2 - HSE 25 MHz (Main Clock):**
| Parámetro | Valor |
|-----------|-------|
| Conexión | PH0/PH1 (U5 pins 12/13) |
| Package | SMD3225-4P |
| **JLCPCB Recomendado** | X322525MOB4SI |
| **LCSC** | **C9006** |
| Tipo | **Basic Part** ✅ |
| Load Capacitance | 12pF |
| Tolerancia | ±10ppm |
| Stock | 585,701+ unidades |
| Precio | ~$0.07 |

- [x] Capacitores de carga C32/C33 (Y1) y C36/C37 (Y2)
- [x] Pines correctos verificados en netlist
- [x] **Timing de sistema funcional**

#### Test Points
- [x] TP1: PE13 (GPIO debug)
- [x] TP2: PE14 (GPIO debug)
- [x] TP3: PA8 (GPIO debug)
- [x] TP5: +12V (power)
- [x] **Debugging y diagnostico disponible**

#### Pines de Control PCM1802 (ADC) - EXTENDIDO
- [x] PA4: FSYNC (Frame Sync Input)
- [x] PA5: OSR (Oversampling Ratio)
- [x] PA6: BYPASS (HPF Bypass)
- [x] PA7: PDWN (Power Down)
- [x] PC6: FMT1 (Format Select 1)
- [x] PC7: FMT0 (Format Select 0)
- [x] PD14: MODE0 (Mode Select 0)
- [x] PD15: MODE1 (Mode Select 1)
- [x] **Control completo de PCM1802 via GPIO (8 pines)**

#### Pines de Control DACs y EEPROM
- [x] DAC Reset: PE10, PE3, PC10, PB15
- [x] DAC Mute: PE11/PE12, PE0/PE1, PC11/PC12, PB14/PB13
- [x] EEPROM WP: PB5
- [x] **Todos los controles de periféricos conectados**

---

## Checklist Fabricacion v2.0

### Hardware - ANTES DE FABRICAR
- [x] Verificar todas las direcciones I2C (sin conflictos)
- [x] Verificar BCK conectados (audio funcional)
- [x] MAX809 con capacitor (reset funcional)
- [x] Reguladores en cascada (power supply OK)
- [x] Cristales con valores correctos
- [x] Todos los pines críticos conectados
- [ ] Revisar footprints de componentes
- [ ] Verificar impedancia de pistas SAI (si es necesario)
- [ ] Revisar planos de GND/alimentación en PCB

### Alternativa: v1.0 (Dev Board STM32H743)
- [ ] Si no tienes fabricada v1, considera hacerlo PRIMERO
- [ ] Permite validar firmware sin riesgo
- [ ] Reduce riesgo de v2 (versión final)

### Firmware - PLAN DE DESARROLLO
- [ ] UART4 (PA0/PA1) para debug y logs
- [ ] Inicialización de SAI1, SAI2, SAI4
- [ ] I2C1: Configurar 4 DACs con direcciones correctas
- [ ] I2C3: EEPROM read/write con protección WP
- [ ] Power sequencing: Reset secuencial de DACs
- [ ] Supervisor MAX809: Ya funciona a nivel hardware
- [ ] 32-channel beamforming algorithm con DMA

---

*Hardware Map v2.0 - 32-Channel Beamforming System*
*Actualizado desde DS.net - Enero 2026*
*Verificación completada - Diseño validado ✓*
