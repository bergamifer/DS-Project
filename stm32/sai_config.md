# STM32H743 SAI Configuration - TDM Mode

## Configuración para DACs (TDM 8 canales por DAC)

### SAI1_A - DAC0 (8 canales)

#### SAI1_ACR1 (Control Register 1)
```c
MODE[1:0]     = 00   // Master transmitter
PRTCFG[1:0]   = 00   // Free protocol (TDM)
DS[2:0]       = 100  // 24-bit data size
LSBFIRST      = 0    // MSB first
CKSTR         = 1    // Data strobing edge: falling edge of SCK
SYNCEN[1:0]   = 00   // Asynchronous mode (genera sus propios clocks)
MONO          = 0    // Stereo/multi-channel mode
OUTDRIV       = 1    // Drive audio block immediately
SAIEN         = 1    // Audio block enable (último paso)
DMAEN         = 1    // DMA enable
NOMCK         = 0    // Master clock enabled
MCKDIV[5:0]   = XX   // Calculado según PLL (ver sección Clock)
```

#### SAI1_AFRCR (Frame Configuration Register)
```c
FRL[7:0]      = 255  // Frame length = 256 bits (32 bits × 8 slots)
FSALL[6:0]    = 0    // Frame sync pulse width = 1 bit
FSDEF         = 0    // FS es start of frame (no channel side)
FSPOL         = 0    // FS active low (falling edge)
FSOFF         = 0    // FS asserted on first bit of slot 0
```

#### SAI1_ASLOTR (Slot Register)
```c
FBOFF[4:0]    = 0    // First bit offset = 0
SLOTSZ[1:0]   = 01   // Slot size = 32 bits
NBSLOT[3:0]   = 7    // Number of slots = 8 (0-7)
SLOTEN[15:0]  = 0x00FF // Slots 0-7 enabled
```

#### SAI1_ACR2 (Control Register 2)
```c
FTH[2:0]      = 010  // FIFO threshold = half full
FFLUSH        = 0    // No FIFO flush
TRIS          = 0    // SD line driven to 0 when inactive
MUTE          = 0    // No mute
MUTEVAL       = 0    // Mute value = 0
COMP[1:0]     = 00   // No companding
```

---

### SAI1_B - DAC1 (8 canales, sincronizado con SAI1_A)

#### SAI1_BCR1 (Control Register 1)
```c
MODE[1:0]     = 00   // Master transmitter
PRTCFG[1:0]   = 00   // Free protocol (TDM)
DS[2:0]       = 100  // 24-bit data size
LSBFIRST      = 0    // MSB first
CKSTR         = 1    // Data strobing edge: falling edge of SCK
SYNCEN[1:0]   = 01   // ⭐ Synchronous with other internal SAI (SAI1_A)
MONO          = 0    // Stereo/multi-channel mode
OUTDRIV       = 1    // Drive audio block immediately
SAIEN         = 1    // Audio block enable
DMAEN         = 1    // DMA enable
NOMCK         = 1    // Master clock disabled (usa MCLK de SAI1_A)
MCKDIV[5:0]   = 0    // Ignorado en sync mode
```

#### SAI1_BFRCR (Frame Configuration Register)
```c
FRL[7:0]      = 255  // Frame length = 256 bits (igual que SAI1_A)
FSALL[6:0]    = 0    // Frame sync pulse width = 1 bit
FSDEF         = 0    // FS es start of frame
FSPOL         = 0    // FS active low
FSOFF         = 0    // FS asserted on first bit
```

#### SAI1_BSLOTR (Slot Register)
```c
FBOFF[4:0]    = 0    // First bit offset = 0
SLOTSZ[1:0]   = 01   // Slot size = 32 bits
NBSLOT[3:0]   = 7    // Number of slots = 8 (0-7)
SLOTEN[15:0]  = 0x00FF // Slots 0-7 enabled
```

#### SAI1_BCR2 (Control Register 2)
```c
FTH[2:0]      = 010  // FIFO threshold = half full
FFLUSH        = 0    // No FIFO flush
TRIS          = 0    // SD line driven to 0 when inactive
MUTE          = 0    // No mute
MUTEVAL       = 0    // Mute value = 0
COMP[1:0]     = 00   // No companding
```

---

### SAI4_B - ADC (2 canales, slave mode o master RX)

**Opción A: ADC como slave (recibe clocks externos)**
Si el PCM1802 genera BCK/LRCK:

#### SAI4_BCR1
```c
MODE[1:0]     = 11   // Slave receiver
PRTCFG[1:0]   = 00   // Free protocol
DS[2:0]       = 100  // 24-bit data size
LSBFIRST      = 0    // MSB first
CKSTR         = 1    // Sample on falling edge
SYNCEN[1:0]   = 00   // Asynchronous
MONO          = 0    // Stereo mode
SAIEN         = 1    // Enable
DMAEN         = 1    // DMA enable
```

**Opción B: SAI4 como master RX (genera clocks para ADC) ⭐ RECOMENDADO**
Si el STM32 genera BCK/LRCK hacia el PCM1802:

#### SAI4_BCR1
```c
MODE[1:0]     = 10   // Master receiver
PRTCFG[1:0]   = 00   // Free protocol
DS[2:0]       = 100  // 24-bit data size
LSBFIRST      = 0    // MSB first
CKSTR         = 1    // Sample on falling edge
SYNCEN[1:0]   = 00   // Asynchronous
MONO          = 0    // Stereo mode
SAIEN         = 1    // Enable
DMAEN         = 1    // DMA enable
NOMCK         = 0    // MCLK enabled (compartido con SAI1)
MCKDIV[5:0]   = XX   // Igual que SAI1_A
```

#### SAI4_BFRCR
```c
FRL[7:0]      = 63   // Frame length = 64 bits (32 × 2 canales)
FSALL[6:0]    = 31   // FS pulse = 32 bits (half frame)
FSDEF         = 1    // FS indica channel side (L/R)
FSPOL         = 0    // FS active low
FSOFF         = 1    // FS one bit before first bit (I2S standard)
```

#### SAI4_BSLOTR
```c
FBOFF[4:0]    = 0    // First bit offset = 0
SLOTSZ[1:0]   = 01   // Slot size = 32 bits
NBSLOT[3:0]   = 1    // Number of slots = 2 (0-1)
SLOTEN[15:0]  = 0x0003 // Slots 0-1 enabled (L+R)
```

---

## Clock Configuration

### Target frequencies @ 48 kHz
```
MCLK  = 12.288 MHz  (256 × 48 kHz)
BCK   = 3.072 MHz   (64 × 48 kHz) para TDM8
LRCK  = 48 kHz      (sample rate)
```

### PLL Configuration (ejemplo con PLL3)

**Entrada:** HSE = 25 MHz (del WeAct board)

**Objetivo:** 12.288 MHz para SAI

**Configuración PLL3:**
```c
// Opción 1: VCO a 344.064 MHz
RCC_PLLCKSELR:
  DIVM3 = 5       // 25 MHz / 5 = 5 MHz ref

RCC_PLL3DIVR:
  DIVN3 = 68      // 5 MHz × 68 = 340 MHz (ajustar a 344.064)
  DIVP3 = 28      // 344.064 MHz / 28 = 12.288 MHz ✓
  DIVQ3 = XX      // No usado
  DIVR3 = XX      // No usado

RCC_PLLCFGR:
  PLL3RGE = 10    // Input range 4-8 MHz
  PLL3VCOSEL = 0  // Wide VCO range (192-960 MHz)
  PLL3FRACEN = 1  // Fractional mode para ajuste fino

RCC_PLL3FRACR:
  FRACN3 = XXXX   // Ajuste fino para exactamente 12.288 MHz
```

**Cálculo MCKDIV:**
```c
// Si SAI clock = 12.288 MHz y queremos MCLK = 12.288 MHz
MCKDIV = 0  // No división adicional

// BCK = MCLK / (FRL+1) × 2
// BCK = 12.288 MHz / 256 × 2 = 96 kHz (incorreto)
// Para TDM, BCK = 64 × fs = 3.072 MHz
// Se genera automáticamente desde MCLK según FRL
```

---

## Secuencia de Inicialización

1. **Configurar GPIO** (Alternate Functions)
2. **Configurar PLL3** para 12.288 MHz
3. **Seleccionar PLL3 como kernel clock para SAI1 y SAI4**
4. **Configurar SAI1_A** (master asíncrono)
5. **Configurar SAI1_B** (master síncrono con A)
6. **Configurar SAI4_B** (slave o master RX)
7. **Configurar DMA** para cada SAI
8. **Habilitar SAI** en orden: SAI4 → SAI1_A → SAI1_B

---

## Timing Constraints

### TDM 8 canales @ 48 kHz
```
Frame period     = 1/48000 = 20.83 μs
Bit clock period = 1/3072000 = 326 ns
Bits per frame   = 256 (32 bits × 8 slots)
Bits per slot    = 32 (24 data + 8 padding)
```

### Data format en memoria
```
Slot 0: [23:0] = Canal 1 data, [31:24] = padding
Slot 1: [23:0] = Canal 2 data, [31:24] = padding
...
Slot 7: [23:0] = Canal 8 data, [31:24] = padding
```

Los datos en el buffer DMA deben estar en formato:
- Right-aligned (bits menos significativos)
- 32-bit word por sample
- Big-endian o Little-endian según configuración del sistema
