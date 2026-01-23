# PCM1690 - Referencia de Configuración

**Fecha:** 2026-01-23
**Dispositivo:** Texas Instruments PCM1690
**Proyecto:** Domosonica DS Audio Processor

---

## Descripción General

DAC de 8 canales, 24-bit, 192 kHz sampling rate con arquitectura Delta-Sigma mejorada y salidas diferenciales.

### Especificaciones Clave

```
SNR:           113 dB (EIAJ, A-weighted @ 48 kHz)
THD+N:         -94 dB (@ 48 kHz, VOUT = 0 dB)
Rango dinámico: 113 dB
Canales:       8 (4 pares estéreo o 8 mono)
Salidas:       Diferenciales, 1.6 × VCC1 Vpp (8 Vpp típico)
Nivel DC:      0.5 × VCC1 (2.5V típico)
```

---

## Alimentación

### Requisitos de Voltaje

```c
VCC1, VCC2:  5.0V ± 0.5V  // Analógico
VDD:         3.3V ± 0.3V  // Digital
```

### Consumo de Corriente @ 48 kHz

```c
ICC (analog):   74 mA típico, 110 mA máximo
IDD (digital):  57 mA típico, 90 mA máximo
Potencia total: 558 mW típico, 847 mW máximo
```

### Esquema de Alimentación

```
[5V Reg] ──┬── VCC1 (pin 25) ──┬── C4 (10μF) ─┬── GND
           │                    └── C1 (1μF)  ─┘
           └── VCC2 (pin 47) ──┬── C5 (10μF) ─┬── GND
                               └── C2 (1μF)  ─┘

[3.3V Reg] ── VDD (pin 12) ────┬── C6 (10μF) ─┬── GND
                               └── C3 (1μF)  ─┘

VCOM (pin 26) ──── C_VCOM (1μF) ──── AGND1
```

**Notas:**
- C1-C3: cerámicos 1μF (X7R o mejor), colocar lo más cerca posible de los pines
- C4-C6: electrolíticos 10μF, baja ESR
- AGND1 (pin 27) y AGND2 (pin 46) conectados al plano analógico
- DGND (pin 13) conectado al plano digital
- Planos analógico y digital unidos en un solo punto

---

## Sistema de Reloj

### Relaciones SCKI/fS Soportadas

| **fS (kHz)** | **SCKI Soportados**                    | **BCK típico**     |
|--------------|----------------------------------------|--------------------|
| 48           | 256fS, 384fS, 512fS, 768fS, 1152fS     | 12.288 MHz (256fS) |
| 96           | 128fS, 256fS, 384fS                    | 12.288 MHz (128fS) |
| 192          | 128fS, 192fS                           | 12.288 MHz (64fS)  |

### Configuración para Domosonica DS @ 48 kHz

```c
fS (LRCK):    48 kHz
SCKI:         12.288 MHz (256fS) ← de PLL3
BCK:          12.288 MHz (256fS)
Modo:         Single Rate (auto)
OSR Digital:  x128
OSR Modulador: x8
```

**Importante:** SCKI, BCK y LRCK deben ser síncronos (misma fuente de reloj). No requieren fase específica pero SÍ relación frecuencial exacta.

---

## Interfaces de Audio Soportadas

### Formato TDM8 (Recomendado para DS)

**Configuración:**
```c
FMTDA[3:0] = 0110  // 24-bit I²S mode TDM (Registro 65, bits 3:0)

// o

FMTDA[3:0] = 0111  // 24-bit Left-justified mode TDM
```

**Temporización TDM @ 48 kHz:**
```
Frame period: 1/48kHz = 20.833 μs
BCK:          256 cycles por frame
Slot width:   32 BCK cycles por canal
Slots:        8 slots (Ch1-Ch8)
Data width:   24 bits por slot
```

**Diagrama TDM8:**
```
LRCK   ───┐                                                      ┌──
          └──────────────────────────────────────────────────────┘
          |<------------------- 256 BCK cycles ------------------>|

BCK    ┐ ┌┐ ┌┐ ┌┐ ┌┐     ┌┐ ┌┐     ┌┐ ┌┐     ┌┐ ┌┐     ┌┐ ┌┐
       └─┘└─┘└─┘└─┘└─...─┘└─┘└─...─┘└─┘└─...─┘└─┘└─...─┘└─┘└─
         |<- Ch1 ->| ....  |<- Ch8 ->|
         32 BCK     ....    32 BCK

DIN1   [23][22]...[0]....[23][22]...[0]
       |<--- Ch1 --->|...|<--- Ch8 --->|
```

**Mapeo de canales en Single Rate:**
```
DIN1: Ch1, Ch2, Ch3, Ch4, Ch5, Ch6, Ch7, Ch8
      └─────────── 256 BCK cycles ─────────────┘
```

### Otras Interfaces Soportadas

| **Formato**      | **Data bits**  | **BCK/frame** | **FMTDA[3:0]** |
|------------------|----------------|---------------|----------------|
| I²S              | 16/20/24/32    | 64, 48, 32    | 0000           |
| Left-justified   | 16/20/24/32    | 64, 48, 32    | 0001           |
| Right-justified  | 24             | 64, 48        | 0010           |
| Right-justified  | 16             | 64, 48, 32    | 0011           |
| DSP I²S mode     | 24             | 64            | 0100           |
| DSP LJ mode      | 24             | 64            | 0101           |

---

## Configuración I²C

### Direcciones (7-bit)

Las direcciones I²C están configuradas por los pines ADR1 y ADR0:

```
Base address: 1 0 0 1 1 [ADR1] [ADR0]

DAC0 (pin 23=0, pin 22=0): 0x4C = 0b1001100
DAC1 (pin 23=1, pin 22=0): 0x4E = 0b1001110
```

**Conexiones en DS Hardware:**
- DAC0_L/R: Dirección 0x4C (ADR1=0, ADR0=0)
- DAC1_L/R: Dirección 0x4E (ADR1=1, ADR0=0)

### Protocolo I²C

**Velocidad soportada:**
- Standard mode: hasta 100 kHz
- Fast mode: hasta 400 kHz

**Formato de escritura:**
```
START | Slave_Addr(7) | W(0) | ACK | Reg_Addr(8) | ACK | Data(8) | ACK | STOP

Ejemplo para escribir 0xFF en registro 72 (atenuación DAC1):
START | 0x4C | 0 | ACK | 0x48 | ACK | 0xFF | ACK | STOP
```

**Escritura múltiple:**
```
START | Slave_Addr | W | ACK | Reg_Addr | ACK | Data1 | ACK | Data2 | ACK | ... | STOP
                                   ↑
                            Auto-incrementa
```

**Formato de lectura:**
```
// Primero escribir dirección del registro
START | Slave_Addr | W | ACK | Reg_Addr | ACK |

// Repeated START y leer
Sr | Slave_Addr | R | ACK | Data | NACK | STOP
```

---

## Mapa de Registros Importantes

### Registro 64 (0x40) - Control del Sistema

```c
typedef struct {
    uint8_t SRDA0  : 1;  // Bit 0: Sampling rate - Auto/Single/Dual/Quad
    uint8_t SRDA1  : 1;  // Bit 1: Sampling rate
    uint8_t AMUTE0 : 1;  // Bit 2: Analog mute por SCKI halt
    uint8_t AMUTE1 : 1;  // Bit 3: Analog mute por async detect
    uint8_t AMUTE2 : 1;  // Bit 4: Analog mute por ZERO detect
    uint8_t AMUTE3 : 1;  // Bit 5: Analog mute por DAC disable
    uint8_t SRST   : 1;  // Bit 6: System reset (auto-clear)
    uint8_t MRST   : 1;  // Bit 7: Mode register reset (auto-clear)
} PCM1690_REG64;

// Valores comunes
#define REG64_DEFAULT       0xC0  // Normal operation
#define REG64_SYSTEM_RESET  0x40  // Trigger system reset
#define REG64_MODE_RESET    0x80  // Trigger mode register reset

// SRDA[1:0] - Sampling Mode
#define SRDA_AUTO   0x00  // Auto-detect (default)
#define SRDA_SINGLE 0x01  // Single rate (fS < 50kHz)
#define SRDA_DUAL   0x02  // Dual rate (fS < 100kHz)
#define SRDA_QUAD   0x03  // Quad rate (fS < 200kHz)
```

### Registro 65 (0x41) - Formato de Audio

```c
typedef struct {
    uint8_t FMTDA0 : 1;  // Bit 0: Audio format
    uint8_t FMTDA1 : 1;  // Bit 1: Audio format
    uint8_t FMTDA2 : 1;  // Bit 2: Audio format
    uint8_t FMTDA3 : 1;  // Bit 3: Audio format
    uint8_t RSV2   : 1;  // Bit 4: Reserved (0)
    uint8_t RSV1   : 1;  // Bit 5: Reserved (0)
    uint8_t RSV0   : 1;  // Bit 6: Reserved (0)
    uint8_t PSMDA  : 1;  // Bit 7: Power-save mode disable
} PCM1690_REG65;

// Valores FMTDA[3:0]
#define FMT_I2S_16_32      0x00  // 16/20/24/32-bit I²S
#define FMT_LJ_16_32       0x01  // 16/20/24/32-bit Left-justified
#define FMT_RJ_24          0x02  // 24-bit Right-justified
#define FMT_RJ_16          0x03  // 16-bit Right-justified
#define FMT_DSP_I2S        0x04  // 24-bit DSP I²S mode
#define FMT_DSP_LJ         0x05  // 24-bit DSP Left-justified
#define FMT_TDM_I2S        0x06  // 24-bit TDM I²S mode ← Para DS
#define FMT_TDM_LJ         0x07  // 24-bit TDM Left-justified

#define REG65_TDM_I2S      0x06  // TDM I²S, Power-save enabled
```

### Registro 66 (0x42) - Control de Operación

```c
typedef struct {
    uint8_t FLT0   : 1;  // Bit 0: DAC1/2 filter roll-off (0=sharp, 1=slow)
    uint8_t FLT1   : 1;  // Bit 1: DAC3/4 filter roll-off
    uint8_t FLT2   : 1;  // Bit 2: DAC5/6 filter roll-off
    uint8_t FLT3   : 1;  // Bit 3: DAC7/8 filter roll-off
    uint8_t OPEDA0 : 1;  // Bit 4: DAC1/2 enable (0=normal, 1=disable)
    uint8_t OPEDA1 : 1;  // Bit 5: DAC3/4 enable
    uint8_t OPEDA2 : 1;  // Bit 6: DAC5/6 enable
    uint8_t OPEDA3 : 1;  // Bit 7: DAC7/8 enable
} PCM1690_REG66;

#define REG66_ALL_ENABLE   0x00  // Todos los DACs enabled, sharp roll-off
#define REG66_ALL_DISABLE  0xF0  // Todos los DACs disabled
```

### Registro 70 (0x46) - De-emphasis y Flags

```c
typedef struct {
    uint8_t ZREV   : 1;  // Bit 0: Zero flag polarity (0=high detect, 1=low)
    uint8_t AZRO0  : 1;  // Bit 1: Zero flag combination
    uint8_t AZRO1  : 1;  // Bit 2: Zero flag combination
    uint8_t RSV    : 1;  // Bit 3: Reserved (0)
    uint8_t DEMP0  : 1;  // Bit 4: De-emphasis (00=off, 01=48k, 10=44.1k, 11=32k)
    uint8_t DEMP1  : 1;  // Bit 5: De-emphasis
    uint8_t RSV2   : 1;  // Bit 6: Reserved (0)
    uint8_t DAMS   : 1;  // Bit 7: Attenuation mode (0=0.5dB step, 1=1dB step)
} PCM1690_REG70;

// AZRO[1:0] - Zero Flag Combination
#define AZRO_COMBO_A  0x00  // ZERO1=DATA1_L, ZERO2=DATA1_R
#define AZRO_COMBO_B  0x02  // ZERO1=DATA1-4, ZERO2=DATA1-4
#define AZRO_COMBO_C  0x04  // ZERO1=DATA4, ZERO2=DATA1-3
#define AZRO_COMBO_D  0x06  // ZERO1=DATA1, ZERO2=DATA2-4

// DEMP[1:0] - De-emphasis
#define DEMP_OFF   0x00  // De-emphasis off (default)
#define DEMP_48K   0x10  // 48 kHz
#define DEMP_441K  0x20  // 44.1 kHz
#define DEMP_32K   0x30  // 32 kHz

#define REG70_DEFAULT  0x00  // De-emphasis off, fine atten, high detect
```

### Registros 72-79 (0x48-0x4F) - Atenuación Digital

```c
// Un registro por canal: ATDA1 a ATDA8
// Fórmula: Attenuation (dB) = Step × (ATDAx - 255)
//          Step = 0.5 dB si DAMS=0, 1.0 dB si DAMS=1

#define ATDA_0DB       0xFF  // Sin atenuación
#define ATDA_05DB      0xFE  // -0.5 dB (DAMS=0)
#define ATDA_10DB      0xFD  // -1.0 dB (DAMS=0) o -1 dB (DAMS=1)
#define ATDA_30DB      0xC7  // -30 dB (DAMS=0)
#define ATDA_63DB      0x81  // -63 dB (DAMS=0)
#define ATDA_MUTE      0x80  // Mute (0x00-0x80 con DAMS=0)

// Cálculo de valor para atenuación deseada (DAMS=0):
// ATDAx = 255 - (dB_atten / 0.5)
// Ejemplo: -20 dB → 255 - 40 = 215 = 0xD7
```

---

## Secuencia de Inicialización Recomendada

### 1. Hardware Reset

```c
// Aplicar reset por hardware
RST_GPIO_LOW();
delay_ms(1);  // Hold reset por mínimo 100ns, pero 1ms es seguro
RST_GPIO_HIGH();
delay_ms(20);  // Esperar 3846 SCKI cycles @ 12.288MHz ≈ 313μs
               // 20ms es mucho más que suficiente
```

### 2. Configuración de Registros

```c
void PCM1690_Init(uint8_t i2c_addr) {
    // Registro 64: Config sistema
    // SRDA=Auto, AMUTE disabled, Normal operation
    PCM1690_WriteReg(i2c_addr, 64, 0x00);

    // Registro 65: Formato audio
    // TDM I²S mode, Power-save enabled
    PCM1690_WriteReg(i2c_addr, 65, FMT_TDM_I2S);

    // Registro 66: Operación
    // Todos los DACs enabled, Sharp roll-off
    PCM1690_WriteReg(i2c_addr, 66, 0x00);

    // Registro 70: De-emphasis off, Fine attenuation
    PCM1690_WriteReg(i2c_addr, 70, 0x00);

    // Registros 72-79: Atenuación inicial 0 dB
    for (uint8_t reg = 72; reg <= 79; reg++) {
        PCM1690_WriteReg(i2c_addr, reg, ATDA_0DB);
    }

    delay_ms(10);  // Tiempo para que los cambios tomen efecto
}

// Inicializar ambos DACs
PCM1690_Init(0x4C);  // DAC0 (Left/Right)
PCM1690_Init(0x4E);  // DAC1 (Left/Right)
```

### 3. Verificación de Configuración

```c
uint8_t PCM1690_Verify(uint8_t i2c_addr) {
    uint8_t reg65_val, reg66_val;

    // Leer registro 65 (formato)
    PCM1690_ReadReg(i2c_addr, 65, &reg65_val);
    if ((reg65_val & 0x0F) != FMT_TDM_I2S) {
        return ERROR_FORMAT_CONFIG;
    }

    // Leer registro 66 (operación)
    PCM1690_ReadReg(i2c_addr, 66, &reg66_val);
    if ((reg66_val & 0xF0) != 0x00) {
        return ERROR_DAC_DISABLED;
    }

    return SUCCESS;
}
```

---

## Salidas Analógicas

### Características Eléctricas

```
Voltaje diferencial full-scale: 1.6 × VCC1 = 8 Vpp (con VCC1=5V)
Nivel DC (cada salida):         0.5 × VCC1 = 2.5V
Impedancia de carga:            ≥5 kΩ (AC-coupled)
                                ≥15 kΩ (DC-coupled)
Capacitancia de carga:          ≤50 pF
```

### Filtro Pasa-Bajos Recomendado

El filtro interno RC del PCM1690 NO es suficiente. Se requiere filtro externo.

**Topología:** Butterworth 2º orden, MFB (Multiple Feedback)

**Opción 1: AC-Coupled (Recomendado para DS)**
```
Component values:
R1 = 7.5 kΩ
R2 = 5.6 kΩ
R3 = 360 Ω
C1 = 3300 pF
C2 = 680 pF
Rf = 47 Ω

Characteristics:
f(-3dB) = 53 kHz
Gain = 0.747 (-2.5 dB)
Order = 2nd Butterworth

Op-amp: OPA2134, OPA2353, o NE5532A
```

**Esquema AC-Coupled:**
```
VOUT+ ──┤├──10μF──┬── R1 ─┬──────────────┐
                  │       │              │
                  │       ├── R3 ──┬─ C2 ┤
                  │       │        │     │
VOUT- ──┤├──10μF──┴── R1 ─┤        └──┬──┴── Rf ──┬── Output
                           │    ┌──────┘          │  (2Vrms)
                           │  [-│+               │
                           └────│ OPA            │
                                └────────────────┘
                           ┌────────┐
                         C1│       │
                           └───────┴─── GND
```

### Mapeo de Salidas

**DAC0 (I²C 0x4C):**
```
VOUT1± (pins 43,44) → Audio Out L1 (Front Left)
VOUT2± (pins 41,42) → Audio Out R1 (Front Right)
VOUT3± (pins 39,40) → Audio Out L2 (opcional)
VOUT4± (pins 37,38) → Audio Out R2 (opcional)
VOUT5± (pins 35,36) → Audio Out L3 (opcional)
VOUT6± (pins 33,34) → Audio Out R3 (opcional)
VOUT7± (pins 31,32) → Audio Out L4 (opcional)
VOUT8± (pins 29,30) → Audio Out R4 (opcional)
```

**DAC1 (I²C 0x4E):**
```
VOUT1± (pins 43,44) → Audio Out L5 (opcional)
VOUT2± (pins 41,42) → Audio Out R5 (opcional)
... (similar para otros canales)
```

---

## Conexiones SAI del STM32H743

### SAI1 Master → PCM1690 DAC0 y DAC1 (TDM)

```c
// SAI1_A (Master Transmitter)
PA3 → SAI1_MCLK_A  (12.288 MHz de PLL3)  →  PCM1690 SCKI (pin 14)
PE2 → SAI1_FS_A                           →  PCM1690 LRCK (pin 6)
PE5 → SAI1_SCK_A                          →  PCM1690 BCK (pin 7)
PE6 → SAI1_SD_A                           →  PCM1690 DIN1 (pin 8) ambos DACs

// Configuración SAI1_A para TDM
Mode:         Master Transmitter
Protocol:     Free (TDM)
Data size:    24 bits
Frame length: 256 bits (8 slots × 32 bits)
Frame sync:   1 BCK cycle active
Clock:        12.288 MHz (256fS @ 48kHz)
Slots:        8 slots activos
Slot size:    32 bits
```

### Resistencias Serie (Reducción EMI)

```c
// Agregar resistencias serie 22-100Ω en las señales:
SAI1_MCLK_A ──[R 47Ω]── SCKI (ambos DACs)
SAI1_FS_A   ──[R 47Ω]── LRCK (ambos DACs)
SAI1_SCK_A  ──[R 47Ω]── BCK  (ambos DACs)
SAI1_SD_A   ──[R 47Ω]── DIN1 (ambos DACs)
```

---

## Control de Volumen y Atenuación

### Atenuación Digital por Canal

Cada canal tiene control independiente de atenuación.

**Modo Fino (DAMS=0, default):**
- Rango: 0 a -63 dB
- Paso: 0.5 dB
- Registros: 72-79 (0x48-0x4F)
- Valores: 0xFF=0dB, 0xFE=-0.5dB, ... 0x81=-63dB, 0x00-0x80=Mute

**Modo Amplio (DAMS=1):**
- Rango: 0 a -100 dB
- Paso: 1.0 dB
- Valores: 0xFF=0dB, 0xFE=-1dB, ... 0x9B=-100dB, 0x00-0x9A=Mute

### Función de Cálculo

```c
/**
 * Calcula el valor de registro ATDAx para una atenuación dada
 * @param atten_db: Atenuación en dB (positivo, ej: 20 para -20dB)
 * @param dams_mode: 0=modo fino (0.5dB), 1=modo amplio (1dB)
 * @return Valor del registro (0-255)
 */
uint8_t PCM1690_CalcAttenuation(float atten_db, uint8_t dams_mode) {
    float step = (dams_mode == 0) ? 0.5f : 1.0f;
    float max_atten = (dams_mode == 0) ? 63.0f : 100.0f;

    // Limitar al rango válido
    if (atten_db < 0.0f) atten_db = 0.0f;
    if (atten_db > max_atten) return 0x00;  // Mute

    // Calcular valor del registro
    int16_t steps = (int16_t)(atten_db / step);
    int16_t reg_val = 255 - steps;

    if (reg_val < 0) reg_val = 0;  // Mute

    return (uint8_t)reg_val;
}

// Ejemplo de uso:
uint8_t atten_20db = PCM1690_CalcAttenuation(20.0f, 0);  // -20dB en modo fino
PCM1690_WriteReg(0x4C, 72, atten_20db);  // Aplicar a canal 1
```

### Soft Mute

El PCM1690 implementa "soft mute" que cambia gradualmente la atenuación para evitar "clicks".

**Velocidad de cambio:** 1 paso (0.5 o 1.0 dB) cada 8/fS

```c
// Ejemplo: Cambio de 0 dB a -20 dB @ 48kHz, modo fino (0.5dB steps)
Steps = 20dB / 0.5dB = 40 steps
Time = 40 steps × (8 / 48000 Hz) = 6.67 ms
```

**Registro 68 (0x44) - Soft Mute Control:**
```c
// Cada bit controla el mute de un canal
// Bit=0: Normal (atenuación según registro 72-79)
// Bit=1: Mute (atenuación infinita, gradual)

#define MUTDA1  (1<<0)  // DAC1 mute
#define MUTDA2  (1<<1)  // DAC2 mute
// ... hasta MUTDA8

// Mutear canal 1 y 2 gradualmente
PCM1690_WriteReg(0x4C, 68, MUTDA1 | MUTDA2);

// Des-mutear (vuelve a atenuación programada)
PCM1690_WriteReg(0x4C, 68, 0x00);
```

---

## Sincronización y Re-sincronización

### Detección de Async

El PCM1690 monitorea continuamente la relación entre SCKI y LRCK.

**Criterio de asincronía:**
- Si la relación SCKI/LRCK cambia más de ±2 BCK cycles → async detectado

**Respuesta a async:**
1. Operación DAC se detiene en 1/fS
2. Salidas VOUTx± forzadas a VCOM (2.5V)
3. Re-sincronización comienza automáticamente
4. Después de 38/fS (single/dual) o 29/fS (quad), vuelve a operación normal

```c
// Tiempos de re-sincronización @ 48 kHz:
// Single/Dual rate: 38/48kHz = 792 μs
// Quad rate:        29/192kHz = 151 μs
```

**Durante el periodo de re-sincronización:**
- Datos de audio son indefinidos
- Puede generarse ruido audible (click/pop)

### SCKI Halt Detection

Si SCKI se detiene:
1. DAC mantiene último estado momentáneamente
2. Si AMUTE[0]=1 (registro 64), salidas van a VCOM
3. Al retornar SCKI, ocurre re-sincronización automática

### Recomendaciones

```c
// Para cambios de frecuencia de muestreo:
// 1. Activar analog mute
PCM1690_WriteReg(i2c_addr, 68, 0xFF);  // Soft mute all
delay_ms(10);  // Esperar que mute tome efecto

// 2. Detener SAI
HAL_SAI_DMAStop(&hsai1);

// 3. Cambiar PLL3 a nueva frecuencia
// ...

// 4. Re-iniciar SAI con nueva config
HAL_SAI_Transmit_DMA(&hsai1, audio_buffer, buffer_size);

// 5. Esperar re-sincronización
delay_ms(2);  // > 792μs @ 48kHz

// 6. Des-activar analog mute
PCM1690_WriteReg(i2c_addr, 68, 0x00);
```

---

## Información Térmica

### PowerPAD

El paquete HTSSOP-48 incluye un PowerPAD térmico expuesto:

```
Dimensiones: 6.40 mm × 3.60 mm
Ubicación:   Debajo del chip (center)
```

**Requisitos de PCB:**
- Soldar PowerPAD directamente a un plano de cobre en el PCB
- Plano de cobre debe estar conectado a AGND
- Agregar vias térmicas para mejorar disipación
- Área mínima recomendada: igual o mayor al PowerPAD

### Resistencia Térmica

```
θJA (Junction-to-Ambient):      29.2 °C/W
θJC (Junction-to-Case top):     10.2 °C/W
θJB (Junction-to-Board):        10.3 °C/W
θJC (Junction-to-Case bottom):   0.4 °C/W  ← con PowerPAD soldado
```

### Cálculo de Temperatura

```c
Tj = Ta + (θJA × Pd)

Ejemplo @ 25°C ambiente, consumo típico:
Pd = 558 mW = 0.558 W
Tj = 25°C + (29.2 °C/W × 0.558W)
Tj = 25°C + 16.3°C = 41.3°C  ← Bien dentro del rango
```

**Temperatura máxima de operación:** 85°C
**Margen con consumo típico:** 85°C - 41.3°C = 43.7°C

---

## Troubleshooting

### No hay salida de audio

**Verificar:**
1. Alimentación: VCC1=5V, VCC2=5V, VDD=3.3V
2. SCKI presente (12.288 MHz para 48kHz)
3. BCK y LRCK síncronos con SCKI
4. RST = HIGH (no en reset)
5. Registro 66: OPEDA[3:0] = 0000 (DACs enabled)
6. Registro 68: MUTDA[8:1] = 0 (no mute)
7. Registros 72-79: ≠ 0x00 (no mute por atenuación)

### Audio distorsionado

**Verificar:**
1. Relación SCKI/LRCK correcta (256fS para TDM8 @ 48kHz)
2. Formato TDM configurado correctamente (Registro 65)
3. Datos de audio válidos en DIN1
4. Filtro pasa-bajos externo presente y funcional
5. Carga de salida dentro de especificaciones (≥5kΩ)

### Clicks o pops en el audio

**Posibles causas:**
1. Cambios bruscos en atenuación → usar soft mute (registro 68)
2. Re-sincronización frecuente → verificar estabilidad de relojes
3. SCKI/BCK/LRCK no síncronos → usar misma fuente de reloj
4. Cambios en configuración sin mute previo

### I²C no responde

**Verificar:**
1. Pull-ups en SDA y SCL (típicamente 4.7kΩ)
2. Dirección I²C correcta (0x4C o 0x4E)
3. Velocidad I²C ≤ 400 kHz
4. VDD presente (3.3V)
5. Formato: 7-bit address, no 8-bit

### Temperatura alta

**Verificar:**
1. PowerPAD soldado correctamente al PCB
2. Plano de cobre debajo del PowerPAD
3. Vias térmicas presentes
4. Flujo de aire alrededor del PCM1690
5. Consumo dentro de especificaciones

---

## Referencias

- **Datasheet:** PCM1690 SBAS448B (Texas Instruments)
- **Application Note:** SLMA002 - PowerPAD Thermally Enhanced Package
- **Application Note:** SLMA004 - PowerPAD Made Easy
- **Application Brief:** SBAA055 - Dynamic Performance Testing of Digital Audio D/A Converters
- **RM0433:** STM32H742, STM32H743/753 and STM32H750 Reference Manual

---

**Documento:** PCM1690 Configuration Reference
**Última actualización:** 2026-01-23
**Versión:** 1.0
