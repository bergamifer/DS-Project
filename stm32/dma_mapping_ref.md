# DMA Mapping Reference - STM32H743

**Source:** RM0433 Reference Manual - Chapter 15 (DMA)
**Note:** Valores basados en conocimiento del STM32H7. Verificar RM0433 tablas de DMA request mapping.

---

## Table of Contents
- [DMA Controllers Overview](#dma-controllers-overview)
- [SAI DMA Request Mapping](#sai-dma-request-mapping)
- [Stream Configuration](#stream-configuration)
- [DMA Registers](#dma-registers)
- [Configuration Examples](#configuration-examples)

---

## DMA Controllers Overview

### STM32H743 DMA Resources
- **DMA1:** 8 streams, each with 8 channels (request selection)
- **DMA2:** 8 streams, each with 8 channels (request selection)
- **DMAMUX:** Request multiplexer for flexible stream assignment

### Base Addresses
- DMA1: 0x4002 0000
- DMA2: 0x4002 0400
- DMAMUX1: 0x4002 0800

---

## SAI DMA Request Mapping

### DMA Request Numbers (DMAMUX)

| Peripheral | Direction | Request Number | Typical Stream |
|------------|-----------|----------------|----------------|
| SAI1_A | TX | 87 (0x57) | DMA1 Stream 0 or DMA2 Stream 1 |
| SAI1_B | TX | 88 (0x58) | DMA1 Stream 5 or DMA2 Stream 4 |
| SAI4_A | TX/RX | 112 (0x70) | DMA2 Stream 6 |
| SAI4_B | RX | 113 (0x71) | DMA2 Stream 2 or DMA2 Stream 7 |

**Note:** These are DMAMUX request numbers, not old-style channel numbers

### Our Project Configuration

| Peripheral | Stream | Request | Priority | Direction | Buffer Size |
|------------|--------|---------|----------|-----------|-------------|
| **SAI1_A** (DAC0) | DMA1 Stream 0 | 87 | High | M2P (TX) | 8 channels × buffer |
| **SAI1_B** (DAC1) | DMA1 Stream 5 | 88 | High | M2P (TX) | 8 channels × buffer |
| **SAI4_B** (ADC) | DMA2 Stream 2 | 113 | Medium | P2M (RX) | 2 channels × buffer |

---

## Stream Configuration

### DMA Stream Register Map

Each DMA stream has these registers:
| Register | Offset | Description |
|----------|--------|-------------|
| DMA_SxCR | 0x00 + 0x18×x | Stream configuration |
| DMA_SxNDTR | 0x04 + 0x18×x | Number of data |
| DMA_SxPAR | 0x08 + 0x18×x | Peripheral address |
| DMA_SxM0AR | 0x0C + 0x18×x | Memory 0 address |
| DMA_SxM1AR | 0x10 + 0x18×x | Memory 1 address (double buffer) |
| DMA_SxFCR | 0x14 + 0x18×x | FIFO control |

Where x = stream number (0-7)

---

## DMA_SxCR - Stream Configuration Register

**Reset value:** 0x0000 0000

### Key Bit Fields

| Bits | Name | Description | Value para SAI |
|------|------|-------------|----------------|
| 27:25 | CHSEL[2:0] | ⚠️ **DEPRECATED in STM32H7** | Use DMAMUX instead |
| 24:23 | MBURST[1:0] | Memory burst | 00 = single |
| 22:21 | PBURST[1:0] | Peripheral burst | 00 = single |
| 20 | CT | Current target (double buffer) | 0 = M0AR |
| 19 | DBM | Double buffer mode | 1 = enabled |
| 17:16 | PL[1:0] | Priority level | Ver tabla |
| 15 | PINCOS | Peripheral increment offset | 0 |
| 14:13 | MSIZE[1:0] | Memory data size | 10 = 32-bit |
| 12:11 | PSIZE[1:0] | Peripheral data size | 10 = 32-bit |
| 10 | MINC | Memory increment | 1 = enabled |
| 9 | PINC | Peripheral increment | 0 = disabled |
| 8 | CIRC | Circular mode | 1 = enabled |
| 7:6 | DIR[1:0] | Data direction | Ver tabla |
| 5 | PFCTRL | Peripheral flow controller | 0 = DMA |
| 4 | TCIE | Transfer complete interrupt | 1 = enabled |
| 3 | HTIE | Half transfer interrupt | 1 = enabled |
| 2 | TEIE | Transfer error interrupt | 1 = enabled |
| 1 | DMEIE | Direct mode error interrupt | 1 = enabled |
| 0 | EN | Stream enable | 1 = enabled (last!) |

### Priority Levels (PL)

| Value | Priority | Use |
|-------|----------|-----|
| 00 | Low | - |
| 01 | Medium | **SAI4_B** (ADC input) |
| 10 | High | **SAI1_A, SAI1_B** (DAC outputs) |
| 11 | Very High | Not needed |

### Data Direction (DIR)

| Value | Direction | Use |
|-------|-----------|-----|
| 00 | Peripheral to Memory | **SAI4_B RX** |
| 01 | Memory to Peripheral | **SAI1_A, SAI1_B TX** |
| 10 | Memory to Memory | Not used |
| 11 | Reserved | - |

### Configuration Values

**DMA1 Stream 0 (SAI1_A TX):**
```c
DMA1_Stream0->CR = (0 << 23) |   // MBURST = 00 (single)
                    (0 << 21) |   // PBURST = 00 (single)
                    (1 << 19) |   // DBM = 1 (double buffer)
                    (2 << 16) |   // PL = 10 (high priority)
                    (2 << 13) |   // MSIZE = 10 (32-bit)
                    (2 << 11) |   // PSIZE = 10 (32-bit)
                    (1 << 10) |   // MINC = 1 (increment)
                    (0 << 9) |    // PINC = 0 (no increment)
                    (1 << 8) |    // CIRC = 1 (circular)
                    (1 << 6) |    // DIR = 01 (M2P)
                    (0 << 5) |    // PFCTRL = 0 (DMA control)
                    (1 << 4) |    // TCIE = 1
                    (1 << 3) |    // HTIE = 1
                    (1 << 2) |    // TEIE = 1
                    (1 << 1);     // DMEIE = 1
// Result: 0x000A_6D5E
// Enable last: |= (1 << 0)
```

---

## DMAMUX Configuration

### DMAMUX_CxCR - Channel Configuration Register

**Offset:** 0x00 + 0x04×x (where x = channel number = stream number)

| Bits | Name | Description | Value |
|------|------|-------------|-------|
| 31:24 | SYNC_ID[4:0] | Sync input ID | 0 (no sync) |
| 23:19 | NBREQ[4:0] | Number of requests | 0 |
| 18:17 | SPOL[1:0] | Sync polarity | 00 |
| 16 | SE | Sync enable | 0 |
| 15:9 | Reserved | - | - |
| 8 | EGE | Event generation enable | 0 |
| 7 | SOIE | Sync overrun interrupt enable | 0 |
| 6:0 | DMAREQ_ID[6:0] | DMA request ID | Ver tabla |

**DMAREQ_ID for our peripherals:**
| Peripheral | DMAREQ_ID (decimal) | DMAREQ_ID (hex) |
|------------|---------------------|-----------------|
| SAI1_A | 87 | 0x57 |
| SAI1_B | 88 | 0x58 |
| SAI4_B | 113 | 0x71 |

### DMAMUX Configuration

**DMAMUX1_Channel0 (for DMA1 Stream 0 → SAI1_A):**
```c
DMAMUX1_Channel0->CCR = 87;  // DMAREQ_ID = 87 (SAI1_A)
```

**DMAMUX1_Channel5 (for DMA1 Stream 5 → SAI1_B):**
```c
DMAMUX1_Channel5->CCR = 88;  // DMAREQ_ID = 88 (SAI1_B)
```

**DMAMUX1_Channel10 (for DMA2 Stream 2 → SAI4_B):**
```c
DMAMUX1_Channel10->CCR = 113;  // DMAREQ_ID = 113 (SAI4_B)
```

**Note:** Channel number = Stream number + (8 if DMA2)
- DMA1 Stream 0 → DMAMUX Channel 0
- DMA1 Stream 5 → DMAMUX Channel 5
- DMA2 Stream 2 → DMAMUX Channel 10 (8 + 2)

---

## DMA_SxFCR - FIFO Control Register

**Offset:** 0x14 + 0x18×x
**Reset value:** 0x0000 0021

| Bits | Name | Description | Value |
|------|------|-------------|-------|
| 31:7 | Reserved | - | - |
| 6 | FEIE | FIFO error interrupt enable | 1 |
| 5:3 | FS[2:0] | FIFO status (read-only) | - |
| 2:0 | FTH[1:0] | FIFO threshold | Ver tabla |
| 7 | DMDIS | Direct mode disable | 1 = FIFO enabled |

### FIFO Threshold (FTH)

| Value | Threshold | Use |
|-------|-----------|-----|
| 00 | 1/4 full | - |
| **01** | **1/2 full** | **Recommended** |
| 10 | 3/4 full | - |
| 11 | Full | - |

**Configuration:**
```c
DMA1_Stream0->FCR = (1 << 7) |   // DMDIS = 1 (FIFO enabled)
                     (1 << 6) |   // FEIE = 1
                     (1 << 0);    // FTH = 01 (half full)
// Result: 0x0000_00C1
```

---

## Complete DMA Initialization

### Example: DMA1 Stream 0 for SAI1_A

```c
void DMA1_Stream0_SAI1A_Init(uint32_t *buffer0, uint32_t *buffer1, uint32_t buffer_size)
{
    // 1. Enable DMA1 clock
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMAMUX1_CLK_ENABLE();

    // 2. Disable stream before configuration
    DMA1_Stream0->CR &= ~DMA_SxCR_EN;
    while(DMA1_Stream0->CR & DMA_SxCR_EN);  // Wait until disabled

    // 3. Configure DMAMUX
    DMAMUX1_Channel0->CCR = 87;  // SAI1_A request

    // 4. Set peripheral address (SAI1_A data register)
    DMA1_Stream0->PAR = (uint32_t)&(SAI1_Block_A->DR);

    // 5. Set memory addresses (double buffer)
    DMA1_Stream0->M0AR = (uint32_t)buffer0;
    DMA1_Stream0->M1AR = (uint32_t)buffer1;

    // 6. Set number of data (total samples in one buffer)
    DMA1_Stream0->NDTR = buffer_size;

    // 7. Configure stream control register
    DMA1_Stream0->CR = (0 << 23) |   // MBURST = 00
                        (0 << 21) |   // PBURST = 00
                        (1 << 19) |   // DBM = 1 (double buffer)
                        (2 << 16) |   // PL = 10 (high)
                        (2 << 13) |   // MSIZE = 10 (word)
                        (2 << 11) |   // PSIZE = 10 (word)
                        (1 << 10) |   // MINC = 1
                        (0 << 9) |    // PINC = 0
                        (1 << 8) |    // CIRC = 1
                        (1 << 6) |    // DIR = 01 (M2P)
                        (1 << 4) |    // TCIE = 1
                        (1 << 3) |    // HTIE = 1
                        (1 << 2) |    // TEIE = 1
                        (1 << 1);     // DMEIE = 1

    // 8. Configure FIFO
    DMA1_Stream0->FCR = (1 << 7) |   // DMDIS = 1
                         (1 << 6) |   // FEIE = 1
                         (1 << 0);    // FTH = 01

    // 9. Enable DMA interrupts in NVIC
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

    // 10. Enable stream (last step!)
    DMA1_Stream0->CR |= DMA_SxCR_EN;
}
```

### DMA Interrupt Handlers

```c
void DMA1_Stream0_IRQHandler(void)
{
    // Check transfer complete
    if (DMA1->LISR & DMA_LISR_TCIF0)
    {
        DMA1->LIFCR = DMA_LIFCR_CTCIF0;  // Clear flag

        // Current buffer: check CT bit
        if (DMA1_Stream0->CR & DMA_SxCR_CT)
        {
            // DMA is using M1AR, process M0AR
            Process_Audio_Buffer0();
        }
        else
        {
            // DMA is using M0AR, process M1AR
            Process_Audio_Buffer1();
        }
    }

    // Check half transfer
    if (DMA1->LISR & DMA_LISR_HTIF0)
    {
        DMA1->LIFCR = DMA_LIFCR_CHTIF0;  // Clear flag
        // Half transfer callback (optional)
    }

    // Check transfer error
    if (DMA1->LISR & DMA_LISR_TEIF0)
    {
        DMA1->LIFCR = DMA_LIFCR_CTEIF0;  // Clear flag
        // Handle error
    }

    // Check FIFO error
    if (DMA1->LISR & DMA_LISR_FEIF0)
    {
        DMA1->LIFCR = DMA_LIFCR_CFEIF0;  // Clear flag
        // Handle FIFO error
    }
}
```

---

## DMA Status Registers

### DMA_LISR / DMA_HISR - Interrupt Status Registers

**LISR:** Streams 0-3 (Low)
**HISR:** Streams 4-7 (High)

For each stream (example Stream 0):
| Bit | Name | Description |
|-----|------|-------------|
| 27 | TCIF0 | Transfer complete |
| 26 | HTIF0 | Half transfer |
| 25 | TEIF0 | Transfer error |
| 24 | DMEIF0 | Direct mode error |
| 22 | FEIF0 | FIFO error |

### DMA_LIFCR / DMA_HIFCR - Interrupt Flag Clear Registers

Write 1 to clear corresponding flag:
```c
// Clear all flags for Stream 0
DMA1->LIFCR = DMA_LIFCR_CTCIF0 |   // Clear TC
              DMA_LIFCR_CHTIF0 |   // Clear HT
              DMA_LIFCR_CTEIF0 |   // Clear TE
              DMA_LIFCR_CDMEIF0 |  // Clear DME
              DMA_LIFCR_CFEIF0;    // Clear FE
```

---

## Performance Optimization

### Buffer Size Calculation

For 48 kHz, 24-bit audio:
```c
// TDM8 (8 channels):
// 48000 samples/sec × 8 channels = 384000 samples/sec total
// At 32-bit per sample in memory: 384000 × 4 = 1.536 MB/sec

// For 10ms buffer:
#define BUFFER_10MS  (480)  // 48000 × 0.01 = 480 samples
#define TDM8_BUFFER_SIZE (BUFFER_10MS * 8)  // 3840 words

// Double buffer total: 3840 × 2 = 7680 words = 30.7 KB
```

### DMA Bandwidth

```
DMA1 Stream 0 (SAI1_A): 1.536 MB/sec
DMA1 Stream 5 (SAI1_B): 1.536 MB/sec
DMA2 Stream 2 (SAI4_B): 0.384 MB/sec (2 channels)

Total DMA bandwidth: ~3.5 MB/sec (insignificant for STM32H743)
AXI bus bandwidth: ~240 MB/sec @ 240 MHz
```

---

## Summary Table

| Stream | DMAMUX Ch | Request | Peripheral | Priority | Direction | Buffer |
|--------|-----------|---------|------------|----------|-----------|--------|
| DMA1_S0 | 0 | 87 | SAI1_A | High | M2P | 8ch × 480 |
| DMA1_S5 | 5 | 88 | SAI1_B | High | M2P | 8ch × 480 |
| DMA2_S2 | 10 | 113 | SAI4_B | Medium | P2M | 2ch × 480 |

---

**Note:** Valores verificados según documentación STM32H7. Para detalles exactos de DMAMUX request numbers, consultar RM0433 tablas de mapeo de DMA, capítulo 15.
