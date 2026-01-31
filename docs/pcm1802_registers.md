# PCM1802 ADC - Guía de Integración con STM32H743

**Documento:** Texas Instruments PCM1802 (SLES023D)
**Tipo:** ADC Stereo 24-bit Delta-Sigma
**Aplicación:** Digitalización de audio para DS Beamforming

---

## 1. Características Principales

| Parámetro | Valor |
|-----------|-------|
| Resolución | 24 bits |
| Canales | 2 (Stereo) |
| Sampling Rate | 16 kHz - 96 kHz |
| THD+N | 0.0015% típico (96 dB) |
| SNR | 105 dB (A-weighted) |
| Dynamic Range | 105 dB |
| Entrada analógica | 3 Vp-p single-ended |
| VCC (Analog) | 5V |
| VDD (Digital) | 3.3V |
| Package | SSOP-20 |

---

## 2. Pinout y Conexiones

```
        ┌──────────────┐
  VINL ─┤ 1         20 ├─ MODE1
  VINR ─┤ 2         19 ├─ MODE0
 VREF1 ─┤ 3         18 ├─ FMT1
 VREF2 ─┤ 4         17 ├─ FMT0
   VCC ─┤ 5         16 ├─ OSR
  AGND ─┤ 6         15 ├─ SCKI (System Clock Input)
  PDWN ─┤ 7         14 ├─ VDD
 BYPAS ─┤ 8         13 ├─ DGND
 FSYNC ─┤ 9         12 ├─ DOUT
  LRCK ─┤ 10        11 ├─ BCK
        └──────────────┘
```

### Conexión recomendada con STM32H743

| Pin PCM1802 | Función | Conexión STM32 | Notas |
|-------------|---------|----------------|-------|
| VINL (1) | Audio In L | Micrófono vía 1µF AC-coupling | Impedancia entrada: 20kΩ |
| VINR (2) | Audio In R | Micrófono vía 1µF AC-coupling | |
| VREF1 (3) | Referencia 1 | 0.1µF + 10µF a GND | Crítico para precisión |
| VREF2 (4) | Referencia 2 | VCC (o vía 1kΩ si supply ruidosa) | |
| VCC (5) | Analog supply | 5V | 0.1µF + 10µF bypass |
| AGND (6) | Analog GND | GND | Conectar con DGND bajo el chip |
| PDWN (7) | Power Down | STM32 GPIO (PA1) | HIGH = Normal, LOW = Power Down |
| BYPAS (8) | HPF Bypass | STM32 GPIO (PC3) o GND | LOW = HPF activo (recomendado) |
| FSYNC (9) | Frame Sync | SAI_FS (input en slave mode) | |
| LRCK (10) | L/R Clock | SAI_FS | fS = sampling frequency |
| BCK (11) | Bit Clock | SAI_SCK | 64 × fS típico |
| DOUT (12) | Data Out | SAI_SD_A | Datos seriales |
| DGND (13) | Digital GND | GND | |
| VDD (14) | Digital supply | 3.3V | 0.1µF + 10µF bypass |
| SCKI (15) | System Clock | SAI_MCLK | 256/384/512/768 × fS |
| OSR (16) | Oversampling | GND o VDD | LOW=×64, HIGH=×128 |
| FMT0 (17) | Format bit 0 | VDD | Para I2S: FMT1=0, FMT0=1 |
| FMT1 (18) | Format bit 1 | GND | |
| MODE0 (19) | Mode bit 0 | GND | Slave mode: ambos a GND |
| MODE1 (20) | Mode bit 1 | GND | |

---

## 3. Configuración por Hardware (Sin Registros Software)

**IMPORTANTE:** El PCM1802 NO tiene registros de software. Toda la configuración es por strapping de pines.

### 3.1 Modo de Operación (MODE1:MODE0)

| MODE1 | MODE0 | Modo | System Clock |
|-------|-------|------|--------------|
| 0 | 0 | **Slave** (recomendado) | 256/384/512/768 fS (auto-detect) |
| 0 | 1 | Master | 512 fS |
| 1 | 0 | Master | 384 fS |
| 1 | 1 | Master | 256 fS |

**Recomendación:** Usar **Slave Mode** (MODE1=MODE0=GND) para que el STM32 sea el master de clocks.

### 3.2 Formato de Datos (FMT1:FMT0)

| FMT1 | FMT0 | Formato |
|------|------|---------|
| 0 | 0 | Left-justified, 24-bit |
| **0** | **1** | **I2S, 24-bit** (recomendado) |
| 1 | 0 | Right-justified, 24-bit |
| 1 | 1 | Right-justified, 20-bit |

**Recomendación:** Usar **I2S 24-bit** (FMT1=GND, FMT0=VDD) por compatibilidad universal.

### 3.3 Oversampling Ratio (OSR)

| OSR | Ratio | Uso |
|-----|-------|-----|
| LOW | ×64 | fS > 50 kHz (96 kHz operation) |
| HIGH | ×128 | fS < 50 kHz (mejor calidad a 48 kHz) |

### 3.4 Control de Power Down (PDWN)

| PDWN | Estado |
|------|--------|
| LOW | Power-down (mínimo consumo) |
| HIGH | Operación normal |

### 3.5 HPF Bypass (BYPAS)

| BYPAS | Modo | Uso |
|-------|------|-----|
| LOW | HPF activo | Remueve DC offset (recomendado para audio) |
| HIGH | HPF bypass | Permite DC en salida (para mediciones) |

---

## 4. Sistema de Clocks - CRÍTICO PARA SINCRONIZACIÓN

### 4.1 Relación de Clocks

```
SCKI (System Clock) = N × fS    donde N = 256, 384, 512, o 768
BCK (Bit Clock) = 64 × fS       (para 2 canales × 32 bits/canal)
LRCK = fS                       (sampling frequency)
```

### 4.2 Frecuencias de System Clock

| fS (kHz) | 256×fS (MHz) | 384×fS (MHz) | 512×fS (MHz) | 768×fS (MHz) |
|----------|--------------|--------------|--------------|--------------|
| 32 | 8.192 | 12.288 | 16.384 | 24.576 |
| 44.1 | 11.2896 | 16.9344 | 22.5792 | 33.8688 |
| **48** | **12.288** | **18.432** | **24.576** | **36.864** |
| 88.2 | 22.5792 | 33.8688 | 45.1584 | — |
| 96 | 24.576 | 36.864 | 49.152 | — |

**Para 48 kHz con oversampling ×128:** SCKI = 12.288 MHz (256×fS)

### 4.3 Timing de System Clock

| Parámetro | Min | Max | Unidad |
|-----------|-----|-----|--------|
| Período SCKI | — | — | — |
| Duración HIGH | 7 | — | ns |
| Duración LOW | 7 | — | ns |

---

## 5. Interfaz de Audio Serial (Slave Mode)

### 5.1 Diagrama de Timing I2S (FMT1:FMT0 = 01)

```
         ┌─────────────────────────────┬─────────────────────────────┐
LRCK     │         LEFT CHANNEL        │        RIGHT CHANNEL        │
         └─────────────────────────────┴─────────────────────────────┘

BCK      ─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─
          └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘
            1   2   3   4  ...  23  24    1   2   3   4  ...  23  24

DOUT       ├MSB─────────────────────LSB├─├MSB─────────────────────LSB├
```

**I2S Características:**
- MSB first
- DOUT cambia en flanco de bajada de BCK
- Datos válidos en flanco de subida de BCK
- 1 BCK de delay después de transición de LRCK

### 5.2 Timing Parameters (Slave Mode)

| Parámetro | Símbolo | Min | Max | Unidad | Descripción |
|-----------|---------|-----|-----|--------|-------------|
| BCK period | t(BCKP) | 150 | — | ns | Período del bit clock |
| BCK high | t(BCKH) | 60 | — | ns | Duración HIGH |
| BCK low | t(BCKL) | 60 | — | ns | Duración LOW |
| LRCK setup | t(LRSU) | 40 | — | ns | Setup time a BCK rising |
| LRCK hold | t(LRHD) | — | 20 | ns | Hold time a BCK rising |
| LRCK period | t(LRCP) | 10 | — | µs | = 1/fS |
| FSYNC setup | t(FSSU) | 20 | — | ns | Setup time a BCK rising |
| FSYNC hold | t(FSHD) | — | 20 | ns | Hold time a BCK rising |
| BCK to DOUT | t(CKDO) | -10 | 20 | ns | Delay BCK↓ a DOUT válido |
| LRCK to DOUT | t(LRDO) | -10 | 20 | ns | Delay LRCK edge a DOUT válido |
| Rise time | tr | — | 10 | ns | |
| Fall time | tf | — | 10 | ns | |

---

## 6. Secuencia de Power-On Reset

```
         ┌────────────────────────────────────────────────────────────
VDD      │  2.6V ─────────────────────────────────────────────────────
         │  2.2V ─ ─ ─ ┬──────────────────────────────────────────────
         │  1.8V       │
─────────┘             │
                       │
Internal   ─────────────┼──────────┬───────────────────────────────────
Reset               RESET         │ RESET REMOVAL
                                  │
                    ├──────────────┼───────────────┤
                    │ 1024 SCKI    │   4480/fS     │
                    │   clocks     │               │

DOUT       ─────────── ZERO DATA ──┼── NORMAL DATA ────────────────────
```

### Tiempos de inicialización

| Fase | Duración | Para 48 kHz |
|------|----------|-------------|
| Reset interno | 1024 SCKI clocks | ~83 µs (@ 12.288 MHz) |
| Datos válidos | 4480 / fS | ~93.3 ms |
| **Total** | | **~93.4 ms** |

**IMPORTANTE:** Esperar al menos **100 ms** después de power-on antes de usar datos del ADC.

---

## 7. Sincronización ADC/DAC - ARQUITECTURA RECOMENDADA

### 7.1 Problema de Sincronización

Para beamforming, es **CRÍTICO** que:
1. Todas las muestras de audio estén perfectamente alineadas en tiempo
2. ADC y DACs compartan el mismo dominio de clock
3. No haya drift entre entrada y salida

### 7.2 Arquitectura de Clocks Recomendada

```
                          ┌─────────────┐
                          │  STM32H743  │
                          │             │
    12.288 MHz            │   SAI_A     │
   ┌───────────┐          │  (Master)   │
   │  Crystal  ├──────────┤             │
   │  / PLL    │          │  MCLK_A ────┼──────┬────────────────────────┐
   └───────────┘          │  SCK_A  ────┼───┬──┼────────────────────┐   │
                          │  FS_A   ────┼─┬─┼──┼────────────────┐   │   │
                          │  SD_A   ←───┼─┼─┼──┼────────────┐   │   │   │
                          │             │ │ │  │            │   │   │   │
                          │   SAI_B     │ │ │  │            │   │   │   │
                          │  (Sync'd)   │ │ │  │            │   │   │   │
                          │  SD_B   ────┼─┼─┼──┼────────┐   │   │   │   │
                          └─────────────┘ │ │  │        │   │   │   │   │
                                          │ │  │        │   │   │   │   │
                   ┌──────────────────────┼─┼──┼────────┼───┼───┼───┼───┤
                   │                      │ │  │        │   │   │   │   │
              ┌────┴────┐            ┌────┴─┴──┴──┐     │   │   │   │   │
              │ PCM1802 │            │  PCM1690   │     │   │   │   │   │
              │  (ADC)  │            │   (DAC)    │     │   │   │   │   │
              │         │            │            │     │   │   │   │   │
              │ SCKI ←──┼────────────┼── SCKI ←───┼─────┘   │   │   │   │
              │ BCK  ←──┼────────────┼── BCK  ←───┼─────────┘   │   │   │
              │ LRCK ←──┼────────────┼── LRCK ←───┼─────────────┘   │   │
              │ DOUT ───┼────────────┼── DATA ←───┼─────────────────┘   │
              │         │            │ MCLK  ←────┼─────────────────────┘
              └─────────┘            └────────────┘

              SLAVE MODE              SLAVE MODE
```

### 7.3 Configuración SAI del STM32H743

```c
// SAI_A Configuration (Master - recibe de PCM1802)
SAI_InitTypeDef sai_init;

sai_init.AudioMode = SAI_MODEMASTER_RX;       // Master receptor
sai_init.Synchro = SAI_ASYNCHRONOUS;          // SAI_A es independiente
sai_init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
sai_init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
sai_init.FIFOThreshold = SAI_FIFOTHRESHOLD_1QF;
sai_init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
sai_init.Protocol = SAI_I2S_STANDARD;         // I2S standard
sai_init.DataSize = SAI_DATASIZE_24;          // 24 bits
sai_init.FirstBit = SAI_FIRSTBIT_MSB;
sai_init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;

// SAI_B Configuration (Synchronized - transmite a PCM1690)
sai_init.AudioMode = SAI_MODESLAVE_TX;        // Slave transmisor
sai_init.Synchro = SAI_SYNCHRONOUS;           // Sincronizado con SAI_A
```

### 7.4 Generación de MCLK

Para 48 kHz con 256×fS:
- MCLK = 48000 × 256 = **12.288 MHz**

Opciones de PLL del STM32H743:
```
HSE = 25 MHz
PLL2 configurado para SAI:
  - DIVM2 = 25 → 1 MHz
  - MULN2 = 344 → 344 MHz
  - DIVP2 = 28 → 12.2857 MHz (error: 0.019%)

O usar cristal externo de 12.288 MHz en SAI_EXTCLK
```

### 7.5 Requisitos de Sincronización (del datasheet)

**CRÍTICO:** En slave mode, si la relación entre LRCK y SCKI cambia más de:
- ±6 BCKs (para BCK = 64×fS)
- ±5 BCKs (para BCK = 48×fS)

...durante un período de muestra, el ADC se detiene y fuerza salida a cero hasta re-sincronización.

**Implicación:** Usar un único PLL para generar todos los clocks. NO usar clocks de diferentes fuentes.

---

## 8. Filtro Interno

### 8.1 Filtro de Decimación

| Parámetro | Valor |
|-----------|-------|
| Pass band | 0 - 0.454 fS |
| Stop band | 0.583 fS - ∞ |
| Pass-band ripple | ±0.05 dB |
| Stop-band attenuation | -65 dB |
| Delay time | 17.4 / fS |

Para fS = 48 kHz:
- Pass band: 0 - 21.8 kHz
- Stop band: 28 kHz+
- Delay: ~362 µs

### 8.2 Filtro High-Pass (HPF)

| Parámetro | Valor |
|-----------|-------|
| Frecuencia -3dB | 0.019 × fS |
| Para 48 kHz | 0.84 Hz |

El HPF remueve el offset DC. Mantener BYPAS = LOW para audio normal.

---

## 9. Consideraciones de Layout PCB

### 9.1 Power Supply
- VCC y VDD: bypass con 0.1µF cerámico + 10µF tantalio **cerca de los pines**
- AGND y DGND: conectar directamente **debajo del chip**

### 9.2 Referencias
- VREF1: 0.1µF + 10µF a AGND, lo más cerca posible
- VREF2: conectar a VCC (opcionalmente vía 1kΩ si supply ruidosa)

### 9.3 Señales Digitales
- DOUT: minimizar capacitancia de carga (<20 pF)
- Clocks: rutear con plano de tierra, evitar crosstalk

### 9.4 Entradas Analógicas
- 1µF AC-coupling en serie (fc ≈ 8 Hz con 20kΩ interno)
- Separar trazas L y R con plano de tierra entre ellas

---

## 10. Ejemplo de Código - Inicialización

```c
// pcm1802.h
#ifndef PCM1802_H
#define PCM1802_H

#include "stm32h7xx_hal.h"

// GPIO Pins (ajustar según tu diseño)
#define PCM1802_PDWN_PORT   GPIOA
#define PCM1802_PDWN_PIN    GPIO_PIN_1
#define PCM1802_BYPASS_PORT GPIOC
#define PCM1802_BYPASS_PIN  GPIO_PIN_3

// Timing
#define PCM1802_POWERUP_DELAY_MS  100  // Esperar después de power-on

typedef struct {
    SAI_HandleTypeDef *hsai;
    uint8_t initialized;
} PCM1802_Handle;

HAL_StatusTypeDef PCM1802_Init(PCM1802_Handle *handle, SAI_HandleTypeDef *hsai);
HAL_StatusTypeDef PCM1802_PowerOn(PCM1802_Handle *handle);
HAL_StatusTypeDef PCM1802_PowerOff(PCM1802_Handle *handle);
void PCM1802_SetHPFBypass(PCM1802_Handle *handle, uint8_t bypass);

#endif
```

```c
// pcm1802.c
#include "pcm1802.h"

HAL_StatusTypeDef PCM1802_Init(PCM1802_Handle *handle, SAI_HandleTypeDef *hsai)
{
    handle->hsai = hsai;
    handle->initialized = 0;

    // Configurar GPIOs
    GPIO_InitTypeDef gpio = {0};

    // PDWN pin
    gpio.Pin = PCM1802_PDWN_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PCM1802_PDWN_PORT, &gpio);

    // BYPASS pin
    gpio.Pin = PCM1802_BYPASS_PIN;
    HAL_GPIO_Init(PCM1802_BYPASS_PORT, &gpio);

    // Iniciar en power-down
    HAL_GPIO_WritePin(PCM1802_PDWN_PORT, PCM1802_PDWN_PIN, GPIO_PIN_RESET);

    // HPF activo (no bypass)
    HAL_GPIO_WritePin(PCM1802_BYPASS_PORT, PCM1802_BYPASS_PIN, GPIO_PIN_RESET);

    return HAL_OK;
}

HAL_StatusTypeDef PCM1802_PowerOn(PCM1802_Handle *handle)
{
    // Activar PDWN (HIGH = normal operation)
    HAL_GPIO_WritePin(PCM1802_PDWN_PORT, PCM1802_PDWN_PIN, GPIO_PIN_SET);

    // Esperar inicialización completa
    // 1024 SCKI clocks + 4480/fS ≈ 100ms para 48kHz
    HAL_Delay(PCM1802_POWERUP_DELAY_MS);

    handle->initialized = 1;
    return HAL_OK;
}

HAL_StatusTypeDef PCM1802_PowerOff(PCM1802_Handle *handle)
{
    HAL_GPIO_WritePin(PCM1802_PDWN_PORT, PCM1802_PDWN_PIN, GPIO_PIN_RESET);
    handle->initialized = 0;
    return HAL_OK;
}

void PCM1802_SetHPFBypass(PCM1802_Handle *handle, uint8_t bypass)
{
    HAL_GPIO_WritePin(PCM1802_BYPASS_PORT, PCM1802_BYPASS_PIN,
                      bypass ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
```

---

## 11. Checklist de Integración

- [ ] Verificar alimentación: VCC = 5V, VDD = 3.3V
- [ ] Condensadores de bypass instalados cerca de pines
- [ ] AGND y DGND conectados bajo el chip
- [ ] FMT1 = GND, FMT0 = VDD (I2S format)
- [ ] MODE1 = MODE0 = GND (Slave mode)
- [ ] OSR configurado según fS (HIGH para <50kHz)
- [ ] MCLK (SCKI) = 256×fS desde STM32 SAI
- [ ] BCK y LRCK generados por STM32 SAI (master)
- [ ] PCM1690 (DAC) usando mismos clocks
- [ ] Delay de 100ms después de power-on antes de usar datos
- [ ] DMA configurado para recepción SAI

---

## 12. Troubleshooting

| Síntoma | Posible Causa | Solución |
|---------|---------------|----------|
| Sin datos | PDWN = LOW | Verificar GPIO |
| Datos = 0 | Clock desincronizado | Verificar SCKI está corriendo |
| Ruido excesivo | Ground loops | Revisar conexión AGND/DGND |
| Offset DC | HPF bypass activo | BYPAS = LOW |
| Distorsión | Señal > 3Vpp | Atenuar entrada |
| Click/pops | Re-sincronización | Verificar estabilidad de clocks |

---

## 13. Verificación de Hardware - DS Beamforming PCB

### 13.1 Estado de Verificación: ✅ APROBADO

Verificado contra netlist `DS.net` - Enero 2026

### 13.2 Configuración Verificada del PCM1802 (U8)

| Pin | Net/Conexión | Valor | Estado |
|-----|--------------|-------|--------|
| FMT0 (17) | +3V3 | HIGH | ✅ |
| FMT1 (18) | GND | LOW | ✅ |
| MODE0 (19) | GND | LOW | ✅ |
| MODE1 (20) | GND | LOW | ✅ |
| VCC (5) | +5VA | 5V analog | ✅ |
| VDD (14) | +3V3 | 3.3V digital | ✅ |
| AGND (6) | GND | — | ✅ |
| DGND (13) | GND | — | ✅ |
| VREF1 (3) | C18 → GND | Bypass cap | ✅ |
| VREF2 (4) | R32→5VA, C19 | Referencia | ✅ |
| SCKI (15) | SAI1_MCLK | Master clock | ✅ |
| BCK (11) | PC1 (SAI4_SCK_B) | Bit clock | ✅ |
| LRCK (10) | PC0 (SAI4_FS_B) | Frame sync | ✅ |
| DOUT (12) | PB11 (SAI4_SD_B) | Data out | ✅ |
| PDWN (7) | /ADC/PDWN | GPIO control | ✅ |
| BYPASS (8) | /ADC/BYPASS | GPIO control | ✅ |
| OSR (16) | /ADC/OSR | GPIO control | ✅ |

**Resultado de configuración:**
- **Modo:** Slave (MODE1=0, MODE0=0)
- **Formato:** I2S 24-bit (FMT1=0, FMT0=1)

### 13.3 Arquitectura de Clocks Verificada

```
STM32H743 PE2 (SAI1_MCLK_A)
         │
         R17 (33Ω) ─── Serie para integridad de señal
         │
         └──── SAI1_MCLK ────┬──── U8 (PCM1802) SCKI
                             ├──── U2 (PCM1690 DAC0) SCKI
                             └──── U3 (PCM1690 DAC1) SCKI

✅ CRÍTICO: Todos los chips comparten el mismo MCLK
   Esto garantiza sincronización perfecta ADC↔DAC
```

### 13.4 Mapeo de Pines STM32H743

| Periférico | Función | Pin STM32 | Destino |
|------------|---------|-----------|---------|
| SAI1 | MCLK_A | PE2 | → Todos SCKI (via R17) |
| SAI4 | FS_B | PC0 | → PCM1802 LRCK |
| SAI4 | SCK_B | PC1 | → PCM1802 BCK |
| SAI4 | SD_B | PB11 | ← PCM1802 DOUT |
| SAI1/2 | FS | PE4 | → PCM1690 U2 LRCK |
| SAI1/2 | SCK | PE5 | → PCM1690 U2 BCK |
| SAI1/2 | SD | PE6 | → PCM1690 U2 DIN1 |
| SAI2 | FS | PE9 | → PCM1690 U3 LRCK |
| SAI2 | SCK | PE8 | → PCM1690 U3 BCK |
| SAI2 | SD | PE7 | → PCM1690 U3 DIN1 |
| I2C1 | SCL | PB8 | → DACs MC/SCL |
| I2C1 | SDA | PB9 | → DACs MD/SDA |

### 13.5 Notas para Firmware

1. **OSR Control:** Configurar GPIO para OSR
   - 48kHz → OSR = HIGH (×128 oversampling, mejor SNR)
   - 96kHz → OSR = LOW (×64 oversampling, requerido)

2. **Power-On Sequence:**
   ```c
   // 1. Iniciar SAI1 MCLK primero
   // 2. Activar PDWN = HIGH
   // 3. Esperar 100ms (1024 clocks + 4480/fS)
   // 4. Datos válidos
   ```

3. **Sincronización SAI:**
   - SAI4 (ADC) y SAI1/SAI2 (DACs) deben compartir clock source
   - Usar PLL2 o PLL3 configurado para 12.288 MHz (256×48kHz)

---

*Documento generado a partir de PCM1802 Datasheet (SLES023D) - Texas Instruments*
*Hardware verificado contra DS.net - Enero 2026*
