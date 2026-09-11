# SAI Registers Reference - STM32H743

**Source:** RM0433 Reference Manual - Chapter 50 (Serial Audio Interface)
**Note:** Valores basados en conocimiento del STM32H7. Verificar RM0433 páginas 2200-2300 para detalles exactos.

---

## Table of Contents
- [Register Summary](#register-summary)
- [SAI_GCR - Global Configuration](#sai_gcr---global-configuration)
- [SAI_xCR1 - Configuration Register 1](#sai_xcr1---configuration-register-1)
- [SAI_xCR2 - Configuration Register 2](#sai_xcr2---configuration-register-2)
- [SAI_xFRCR - Frame Configuration](#sai_xfrcr---frame-configuration)
- [SAI_xSLOTR - Slot Register](#sai_xslotr---slot-register)
- [SAI_xIM - Interrupt Mask](#sai_xim---interrupt-mask)
- [SAI_xSR - Status Register](#sai_xsr---status-register)
- [SAI_xCLRFR - Clear Flag Register](#sai_xclrfr---clear-flag-register)
- [SAI_xDR - Data Register](#sai_xdr---data-register)
- [Configuration Examples](#configuration-examples)

---

## Register Summary

| Register | Offset SAI1_A | Offset SAI1_B | Offset SAI4_B | Description |
|----------|---------------|---------------|---------------|-------------|
| SAI_GCR  | 0x00 | 0x00 | 0x00 | Global configuration (shared) |
| SAI_ACR1/BCR1 | 0x04 | 0x24 | 0x24 | Configuration register 1 |
| SAI_ACR2/BCR2 | 0x08 | 0x28 | 0x28 | Configuration register 2 |
| SAI_AFRCR/BFRCR | 0x0C | 0x2C | 0x2C | Frame configuration |
| SAI_ASLOTR/BSLOTR | 0x10 | 0x30 | 0x30 | Slot register |
| SAI_AIM/BIM | 0x14 | 0x34 | 0x34 | Interrupt mask |
| SAI_ASR/BSR | 0x18 | 0x38 | 0x38 | Status register |
| SAI_ACLRFR/BCLRFR | 0x1C | 0x3C | 0x3C | Clear flag register |
| SAI_ADR/BDR | 0x20 | 0x40 | 0x40 | Data register |

**Base Addresses:**
- SAI1: 0x4001 5800
- SAI4: 0x5800 B400

---

## SAI_GCR - Global Configuration

**Offset:** 0x00
**Reset value:** 0x0000 0000

| Bits | Name | Type | Description | Value para proyecto |
|------|------|------|-------------|---------------------|
| 31:6 | Reserved | - | - | - |
| 5:4 | SYNCOUT[1:0] | RW | Synchronization outputs | 00 (no sync out) |
| 3:2 | SYNCIN[1:0] | RW | Synchronization inputs | Ver tabla |
| 1:0 | Reserved | - | - | - |

**SYNCIN Values (for SAI1):**
| Value | Source | Use |
|-------|--------|-----|
| 00 | No sync input | Default |
| 01 | SAI2 sync | Not available |
| 10 | SAI3 sync | External SAI |
| 11 | SAI4 sync | External SAI |

**SYNCOUT Values:**
| Value | Output | Use |
|-------|--------|-----|
| 00 | No output | Default (internal sync only) |
| 01 | Block A | Sync output from SAI_A |
| 10 | Block B | Sync output from SAI_B |
| 11 | Reserved | - |

**Configuration for our project:**
```c
// SAI1: No external sync needed (internal sync between A and B)
SAI1->GCR = 0x00000000;

// SAI4: Independent from SAI1
SAI4->GCR = 0x00000000;
```

---

## SAI_xCR1 - Configuration Register 1

**Offset:** 0x04 (Block A), 0x24 (Block B)
**Reset value:** 0x0000 0040

| Bits | Name | Type | Description | Values |
|------|------|------|-------------|--------|
| 31:20 | Reserved | - | - | - |
| 19:17 | MCKDIV[5:3] | RW | Master clock divider MSB | Ver cálculo |
| 16 | NODIV | RW | No divider | 0 = MCLK enabled |
| 15 | DMAEN | RW | DMA enable | 1 = enabled |
| 14 | Reserved | - | - | - |
| 13 | SAIEN | RW | SAI block enable | 1 = enabled (last step!) |
| 12 | OUTDRIV | RW | Output drive | 1 = immediately |
| 11 | MONO | RW | Mono mode | 0 = stereo/multi-ch |
| 10:9 | SYNCEN[1:0] | RW | Synchronization enable | Ver tabla |
| 8 | CKSTR | RW | Clock strobing edge | 1 = falling edge |
| 7 | LSBFIRST | RW | LSB first | 0 = MSB first |
| 6:5 | DS[2:0] | RW | Data size | Ver tabla |
| 4:2 | PRTCFG[1:0] | RW | Protocol configuration | 00 = free (TDM) |
| 1:0 | MODE[1:0] | RW | SAI block mode | Ver tabla |

### MODE[1:0] - Block Mode

| Value | Mode | Description |
|-------|------|-------------|
| 00 | Master transmitter | **DAC0, DAC1** |
| 01 | Master receiver | **ADC** (if SAI generates clocks) |
| 10 | Slave transmitter | Not used |
| 11 | Slave receiver | ADC (if external clocks) |

### PRTCFG[1:0] - Protocol

| Value | Protocol | Use |
|-------|----------|-----|
| **00** | **Free protocol** | **TDM/I2S custom** |
| 01 | SPDIF | Not used |
| 10 | AC'97 | Not used |
| 11 | Reserved | - |

### DS[2:0] - Data Size

| Value | Size | Use |
|-------|------|-----|
| 000 | 8-bit | - |
| 001 | 10-bit | - |
| 010 | 16-bit | - |
| 011 | 20-bit | - |
| **100** | **24-bit** | **Our project** |
| 101 | 32-bit | - |
| 11x | Reserved | - |

### SYNCEN[1:0] - Synchronization

| Value | Mode | Use |
|-------|------|-----|
| **00** | **Asynchronous** | **SAI1_A, SAI4_B** |
| **01** | **Internal sync** | **SAI1_B** (sync with SAI1_A) |
| 10 | External SAI sync | Not used |
| 11 | Reserved | - |

### MCKDIV Calculation

```
MCLK = SAI_CK / (MCKDIV × 2) if NODIV = 0

For our case:
SAI_CK = 12.288 MHz (from PLL3_P)
Target MCLK = 12.288 MHz
MCKDIV = SAI_CK / (MCLK × 2)
MCKDIV = 12.288 / (12.288 × 2) = 0.5

Since MCKDIV must be ≥ 1, use MCKDIV = 0 (special case)
When MCKDIV = 0, MCLK = SAI_CK directly

Actually, the formula when NODIV = 0:
- If MCKDIV = 0, then MCLK is disabled OR
- MCLK = SAI_CK / (2 × (MCKDIV + 1))

For MCLK = SAI_CK, we need:
12.288 = 12.288 / (2 × (MCKDIV + 1))
2 × (MCKDIV + 1) = 1
MCKDIV + 1 = 0.5
This is impossible.

Alternative: Set NODIV = 1 to bypass divider
When NODIV = 1, MCLK = SAI_CK directly
```

**Solution:**
```c
NODIV = 0 (enable MCLK output)
MCKDIV = 0 (special case for SAI_CK / 2)
// OR
NODIV = 1 (MCLK = SAI_CK, but check if this affects BCK generation)
```

**Note:** Need to verify in RM0433 the exact behavior of MCKDIV = 0 vs NODIV = 1

### Configuration Values

**SAI1_A (DAC0 - Master TX, Asynchronous):**
```c
SAI1_Block_A->CR1 = (0 << 19) |     // MCKDIV[5:3] = 0
                     (0 << 17) |     // MCKDIV[2:0] = 0
                     (0 << 16) |     // NODIV = 0 (MCLK enabled)
                     (1 << 15) |     // DMAEN = 1
                     (1 << 12) |     // OUTDRIV = 1
                     (0 << 11) |     // MONO = 0 (stereo)
                     (0 << 9) |      // SYNCEN = 00 (asynchronous)
                     (1 << 8) |      // CKSTR = 1 (falling edge)
                     (0 << 7) |      // LSBFIRST = 0 (MSB first)
                     (4 << 5) |      // DS = 100 (24-bit)
                     (0 << 2) |      // PRTCFG = 00 (free)
                     (0 << 0);       // MODE = 00 (master TX)
// Result: 0x0000_9900
// + Enable last: |= (1 << 13)  // SAIEN = 1
```

**SAI1_B (DAC1 - Master TX, Synchronized):**
```c
SAI1_Block_B->CR1 = (0 << 19) |     // MCKDIV = 0 (ignored in sync mode)
                     (1 << 16) |     // NODIV = 1 (no MCLK output needed)
                     (1 << 15) |     // DMAEN = 1
                     (1 << 12) |     // OUTDRIV = 1
                     (0 << 11) |     // MONO = 0
                     (1 << 9) |      // SYNCEN = 01 (internal sync)
                     (1 << 8) |      // CKSTR = 1
                     (0 << 7) |      // LSBFIRST = 0
                     (4 << 5) |      // DS = 100 (24-bit)
                     (0 << 2) |      // PRTCFG = 00
                     (0 << 0);       // MODE = 00 (master TX)
// Result: 0x0001_9B00
```

**SAI4_B (ADC - Master RX, Asynchronous):**
```c
SAI4_Block_B->CR1 = (0 << 19) |     // MCKDIV = 0
                     (0 << 16) |     // NODIV = 0 (MCLK enabled)
                     (1 << 15) |     // DMAEN = 1
                     (1 << 12) |     // OUTDRIV = 1
                     (0 << 11) |     // MONO = 0
                     (0 << 9) |      // SYNCEN = 00 (asynchronous)
                     (1 << 8) |      // CKSTR = 1
                     (0 << 7) |      // LSBFIRST = 0
                     (4 << 5) |      // DS = 100 (24-bit)
                     (0 << 2) |      // PRTCFG = 00
                     (1 << 0);       // MODE = 01 (master RX)
// Result: 0x0000_9901
```

---

## SAI_xCR2 - Configuration Register 2

**Offset:** 0x08 (Block A), 0x28 (Block B)
**Reset value:** 0x0000 0000

| Bits | Name | Type | Description | Value |
|------|------|------|-------------|-------|
| 31:16 | Reserved | - | - | - |
| 15:14 | COMP[1:0] | RW | Companding mode | 00 = no companding |
| 13 | CPL | RW | Complement bit | 0 |
| 12:7 | MUTECNT[5:0] | RW | Mute counter | 0 (no auto-mute) |
| 6 | MUTEVAL | RW | Mute value | 0 |
| 5 | MUTE | RW | Mute | 0 = no mute |
| 4 | TRIS | RW | Tristate management | 0 = SD line driven |
| 3 | FFLUSH | RW | FIFO flush | 0 (flush with WO pulse) |
| 2:0 | FTH[2:0] | RW | FIFO threshold | Ver tabla |

### FTH[2:0] - FIFO Threshold

| Value | Threshold | TX: Int when | RX: Int when |
|-------|-----------|--------------|--------------|
| 000 | Empty | FIFO empty | ≥1 data |
| 001 | 1/4 full | <1/4 full | ≥1/4 full |
| **010** | **1/2 full** | **<1/2 full** | **≥1/2 full** |
| 011 | 3/4 full | <3/4 full | ≥3/4 full |
| 100 | Full | Not full | Full |
| Others | Reserved | - | - |

**Configuration:**
```c
// SAI1_A, SAI1_B, SAI4_B (all same)
SAIx_Block->CR2 = (0 << 14) |    // COMP = 00 (no companding)
                   (0 << 13) |    // CPL = 0
                   (0 << 7) |     // MUTECNT = 0
                   (0 << 6) |     // MUTEVAL = 0
                   (0 << 5) |     // MUTE = 0
                   (0 << 4) |     // TRIS = 0 (drive SD)
                   (2 << 0);      // FTH = 010 (half full)
// Result: 0x0000_0002
```

---

## SAI_xFRCR - Frame Configuration

**Offset:** 0x0C (Block A), 0x2C (Block B)
**Reset value:** 0x0000 0007

| Bits | Name | Type | Description | Value |
|------|------|------|-------------|-------|
| 31:19 | Reserved | - | - | - |
| 18 | FSOFF | RW | Frame sync offset | Ver tabla |
| 17 | FSPOL | RW | Frame sync polarity | 0 = active low |
| 16 | FSDEF | RW | Frame sync definition | Ver tabla |
| 15:8 | FSALL[6:0] | RW | Frame sync active length | Ver tabla |
| 7:0 | FRL[7:0] | RW | Frame length | Ver tabla |

### FSOFF - Frame Sync Offset

| Value | Position | Use |
|-------|----------|-----|
| 0 | FS on first bit of frame | **TDM, DSP** |
| 1 | FS one bit before first bit | **I2S standard** |

### FSDEF - Frame Sync Definition

| Value | Type | Use |
|-------|------|-----|
| 0 | Start of frame | **TDM** (no L/R indication) |
| 1 | Channel side (L/R) | **I2S** (FS indicates channel) |

### Frame Configuration for TDM8 (DACs)

```
Frame length = 256 bits (8 slots × 32 bits)
FRL = 255 (frame length - 1)
FSALL = 0 (1-bit pulse)
FSDEF = 0 (start of frame only)
FSPOL = 0 (active low)
FSOFF = 0 (on first bit)
```

**SAI1_A / SAI1_B (TDM8):**
```c
SAIx_Block->FRCR = (0 << 18) |     // FSOFF = 0 (on first bit)
                    (0 << 17) |     // FSPOL = 0 (active low)
                    (0 << 16) |     // FSDEF = 0 (start of frame)
                    (0 << 8) |      // FSALL = 0 (1-bit pulse)
                    (255 << 0);     // FRL = 255 (256 bits total)
// Result: 0x0000_00FF
```

### Frame Configuration for I2S (ADC)

```
Frame length = 64 bits (2 channels × 32 bits)
FRL = 63 (frame length - 1)
FSALL = 31 (32-bit pulse = half frame)
FSDEF = 1 (L/R indication)
FSPOL = 0 (active low)
FSOFF = 1 (one bit before)
```

**SAI4_B (I2S 2-channel):**
```c
SAI4_Block_B->FRCR = (1 << 18) |     // FSOFF = 1 (before first bit)
                      (0 << 17) |     // FSPOL = 0 (active low)
                      (1 << 16) |     // FSDEF = 1 (channel side)
                      (31 << 8) |     // FSALL = 31 (32-bit pulse)
                      (63 << 0);      // FRL = 63 (64 bits total)
// Result: 0x0005_1F3F
```

---

## SAI_xSLOTR - Slot Register

**Offset:** 0x10 (Block A), 0x30 (Block B)
**Reset value:** 0x0000 0000

| Bits | Name | Type | Description | Value |
|------|------|------|-------------|-------|
| 31:16 | SLOTEN[15:0] | RW | Slot enable | Bit mask of active slots |
| 15:12 | NBSLOT[3:0] | RW | Number of slots | (num_slots - 1) |
| 11:8 | SLOTSZ[1:0] | RW | Slot size | Ver tabla |
| 7:0 | FBOFF[4:0] | RW | First bit offset | Usually 0 |

### SLOTSZ[1:0] - Slot Size

| Value | Size | Use |
|-------|------|-----|
| 00 | = DS (from CR1) | Data size = slot size |
| **01** | **32 bits** | **Our choice** (24-bit data in 32-bit slot) |
| 10 | 16 bits | - |
| 11 | Reserved | - |

### Slot Configuration for TDM8 (DACs)

```
Number of slots = 8 (0-7)
NBSLOT = 7 (8 - 1)
SLOTSZ = 01 (32-bit slots)
SLOTEN = 0x00FF (enable slots 0-7)
FBOFF = 0 (no offset)
```

**SAI1_A / SAI1_B (TDM8):**
```c
SAIx_Block->SLOTR = (0x00FF << 16) |  // SLOTEN = 0x00FF (slots 0-7)
                     (7 << 12) |       // NBSLOT = 7 (8 slots)
                     (1 << 8) |        // SLOTSZ = 01 (32-bit)
                     (0 << 0);         // FBOFF = 0
// Result: 0x00FF_7100
```

### Slot Configuration for I2S (ADC)

```
Number of slots = 2 (L, R)
NBSLOT = 1 (2 - 1)
SLOTSZ = 01 (32-bit slots)
SLOTEN = 0x0003 (enable slots 0-1)
FBOFF = 0
```

**SAI4_B (I2S 2-channel):**
```c
SAI4_Block_B->SLOTR = (0x0003 << 16) |  // SLOTEN = 0x0003 (slots 0-1)
                       (1 << 12) |       // NBSLOT = 1 (2 slots)
                       (1 << 8) |        // SLOTSZ = 01 (32-bit)
                       (0 << 0);         // FBOFF = 0
// Result: 0x0003_1100
```

---

## SAI_xIM - Interrupt Mask

**Offset:** 0x14 (Block A), 0x34 (Block B)
**Reset value:** 0x0000 0000

| Bits | Name | Type | Description |
|------|------|------|-------------|
| 31:7 | Reserved | - | - |
| 6 | LFSDETIE | RW | Late frame sync detection interrupt enable |
| 5 | AFSDETIE | RW | Anticipated frame sync detection interrupt enable |
| 4 | CNRDYIE | RW | Codec not ready interrupt enable |
| 3 | FREQIE | RW | FIFO request interrupt enable |
| 2 | WCKCFGIE | RW | Wrong clock configuration interrupt enable |
| 1 | MUTEDETIE | RW | Mute detection interrupt enable |
| 0 | OVRUDRIE | RW | Overrun/underrun interrupt enable |

**Typical Configuration (DMA mode, minimal interrupts):**
```c
SAIx_Block->IM = (1 << 6) |  // LFSDETIE = 1 (detect late FS)
                  (1 << 5) |  // AFSDETIE = 1 (detect early FS)
                  (1 << 0);   // OVRUDRIE = 1 (detect overrun/underrun)
// Result: 0x0000_0061
// Note: FREQIE not needed with DMA
```

---

## SAI_xSR - Status Register

**Offset:** 0x18 (Block A), 0x38 (Block B)
**Reset value:** 0x0000 0008

| Bits | Name | Type | Description |
|------|------|------|-------------|
| 31:19 | Reserved | - | - |
| 18:16 | FLVL[2:0] | R | FIFO level (read-only) |
| 15:7 | Reserved | - | - |
| 6 | LFSDET | R | Late frame sync detection |
| 5 | AFSDET | R | Anticipated frame sync detection |
| 4 | CNRDY | R | Codec not ready |
| 3 | FREQ | R | FIFO request |
| 2 | WCKCFG | R | Wrong clock configuration |
| 1 | MUTEDET | R | Mute detection |
| 0 | OVRUDR | R | Overrun/underrun |

**FLVL Values (TX mode):**
| Value | FIFO status |
|-------|-------------|
| 000 | Empty |
| 001 | 1/4 full |
| 010 | 1/2 full |
| 011 | 3/4 full |
| 100 | Full |
| 101 | Full |
| Others | Reserved |

---

## SAI_xCLRFR - Clear Flag Register

**Offset:** 0x1C (Block A), 0x3C (Block B)
**Reset value:** 0x0000 0000
**Type:** Write-only

| Bits | Name | Type | Description |
|------|------|------|-------------|
| 31:7 | Reserved | - | - |
| 6 | CLFSDET | W | Clear late FS flag |
| 5 | CAFSDET | W | Clear anticipated FS flag |
| 4 | CCNRDY | W | Clear codec not ready flag |
| 3 | Reserved | - | - |
| 2 | CWCKCFG | W | Clear wrong clock config flag |
| 1 | CMUTEDET | W | Clear mute detect flag |
| 0 | COVRUDR | W | Clear overrun/underrun flag |

**Usage:**
```c
// Clear all error flags
SAIx_Block->CLRFR = (1 << 6) |  // CLFSDET
                     (1 << 5) |  // CAFSDET
                     (1 << 4) |  // CCNRDY
                     (1 << 2) |  // CWCKCFG
                     (1 << 1) |  // CMUTEDET
                     (1 << 0);   // COVRUDR
```

---

## SAI_xDR - Data Register

**Offset:** 0x20 (Block A), 0x40 (Block B)
**Reset value:** 0x0000 0000

| Bits | Name | Type | Description |
|------|------|------|-------------|
| 31:0 | DATA[31:0] | RW | Data |

**Data Alignment:**
- For 24-bit data in 32-bit slots: right-aligned
- Upper 8 bits ignored on TX, read as 0 on RX
- Example: `0x00ABCDEF` for 24-bit value 0xABCDEF

---

## Configuration Examples

### Example 1: SAI1_A Complete Setup (DAC0, TDM8)

```c
void SAI1_BlockA_TDM8_Init(void)
{
    // 1. Enable SAI1 clock
    __HAL_RCC_SAI1_CLK_ENABLE();

    // 2. Configure CR1 (but don't enable yet)
    SAI1_Block_A->CR1 = (0 << 19) |   // MCKDIV = 0
                         (0 << 16) |   // NODIV = 0 (MCLK enabled)
                         (1 << 15) |   // DMAEN = 1
                         (1 << 12) |   // OUTDRIV = 1
                         (0 << 11) |   // MONO = 0
                         (0 << 9) |    // SYNCEN = 00 (async)
                         (1 << 8) |    // CKSTR = 1
                         (0 << 7) |    // LSBFIRST = 0
                         (4 << 5) |    // DS = 100 (24-bit)
                         (0 << 2) |    // PRTCFG = 00
                         (0 << 0);     // MODE = 00 (master TX)

    // 3. Configure CR2
    SAI1_Block_A->CR2 = (0 << 14) |   // COMP = 00
                         (2 << 0);     // FTH = 010 (half full)

    // 4. Configure FRCR
    SAI1_Block_A->FRCR = (0 << 18) |  // FSOFF = 0
                          (0 << 17) |  // FSPOL = 0
                          (0 << 16) |  // FSDEF = 0
                          (0 << 8) |   // FSALL = 0
                          (255 << 0);  // FRL = 255

    // 5. Configure SLOTR
    SAI1_Block_A->SLOTR = (0x00FF << 16) |  // SLOTEN = 0xFF
                           (7 << 12) |       // NBSLOT = 7
                           (1 << 8) |        // SLOTSZ = 01
                           (0 << 0);         // FBOFF = 0

    // 6. Configure interrupts (optional)
    SAI1_Block_A->IM = (1 << 6) |   // LFSDETIE
                        (1 << 5) |   // AFSDETIE
                        (1 << 0);    // OVRUDRIE

    // 7. Enable SAI (last step, after DMA setup)
    // SAI1_Block_A->CR1 |= (1 << 13);  // SAIEN = 1
}
```

### Example 2: Actual Frequency Calculations

```
SAI_CK = 12.288 MHz (from PLL3)

For TDM8 @ 48 kHz:
- Frame length = 256 bits
- LRCK (fs) = 48 kHz
- BCK = LRCK × frame_length = 48000 × 256 = 12.288 MHz

BCK is generated from MCLK:
If MCLK = SAI_CK = 12.288 MHz
And we need BCK = 12.288 MHz
Then BCK = MCLK (no division)

This matches TDM requirements where BCK = bits_per_frame × sample_rate

For I2S @ 48 kHz:
- Frame length = 64 bits (2 channels × 32 bits)
- LRCK = 48 kHz
- BCK = 48000 × 64 = 3.072 MHz
- MCLK = 256 × 48000 = 12.288 MHz

BCK = MCLK / 4 in this case
```

---

**Note:** Todos los valores han sido calculados basándose en conocimiento del STM32H7. Para verificar offsets exactos, bits reservados y reset values, consultar RM0433 capítulo 50 (SAI) páginas 2200-2300.
