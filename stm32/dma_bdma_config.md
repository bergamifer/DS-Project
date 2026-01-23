# DMA and BDMA Mapping Reference - STM32H743

**Source:** RM0433 Reference Manual
- Chapter 15: DMA Controllers (DMA1, DMA2)
- Chapter 16: BDMA Controller
- Chapter 17: DMAMUX request multiplexer

**Verified:** 2026-01-22 against RM0433
- Table 102: DMAMUX1 & DMA1/DMA2 connections (RM0433 line 91827)
- Table 103: DMAMUX2 & BDMA connections (RM0433 line 92903)
- Table 122: DMAMUX1 request assignments (RM0433 line 111894)
- Table 126: DMAMUX2 request assignments (RM0433 line 113114)

---

## ⚠️ CRITICAL CORRECTION

**❌ Previous documentation was INCORRECT:**
- SAI4_B listed as using DMA2 Stream 2 with DMAMUX1 request 113

**✅ CORRECT information:**
- **SAI4 is in D3 domain (APB4) and MUST use BDMA with DMAMUX2**
- SAI4_A: BDMA Channel with DMAMUX2 request 15
- SAI4_B: BDMA Channel with DMAMUX2 request 16

---

## Table of Contents
- [DMA Controllers Overview](#dma-controllers-overview)
- [SAI DMA/BDMA Mapping](#sai-dmabdma-mapping)
- [DMA1/DMA2 Configuration (SAI1)](#dma1dma2-configuration-sai1)
- [BDMA Configuration (SAI4)](#bdma-configuration-sai4)
- [Complete Examples](#complete-examples)

---

## DMA Controllers Overview

### STM32H743 has THREE DMA controllers

| Controller | Type | Channels/Streams | Domain | DMAMUX | Base Address | Peripherals |
|------------|------|------------------|--------|--------|--------------|-------------|
| **DMA1** | Advanced | 8 streams | D2 | DMAMUX1 | 0x40020000 | **SAI1**, SAI2, SAI3, SPI1-3, I2C1-3, UART, TIM |
| **DMA2** | Advanced | 8 streams | D2 | DMAMUX1 | 0x40020400 | **SAI1**, SAI2, SAI3, ADC1/2, DCMI, CRYP, etc |
| **BDMA** | Basic | 8 channels | **D3** | **DMAMUX2** | **0x58025400** | **SAI4**, LPUART, SPI6, I2C4, ADC3 |

### Key Differences

**DMA1/DMA2 (Advanced DMA):**
- 8 streams per controller
- FIFO support
- Burst transfers
- Complex features
- Located in D2 domain

**BDMA (Basic DMA):**
- 8 channels (simpler than streams)
- No FIFO
- No burst transfers
- Simpler, lower power
- **Located in D3 domain** (critical for SAI4!)

### DMAMUX Base Addresses

- **DMAMUX1:** 0x40020800 (for DMA1/DMA2)
- **DMAMUX2:** 0x58003800 (for BDMA)

---

## SAI DMA/BDMA Mapping

### ✅ SAI1 (D2 domain) → DMA1/DMA2 + DMAMUX1

**Source:** RM0433 Table 122 (line 111894)

| Peripheral | DMAMUX1 Request | Direction | Typical Configuration |
|------------|-----------------|-----------|----------------------|
| SAI1_A | **87** (0x57) | TX | DMA1 Stream 0 |
| SAI1_B | **88** (0x58) | TX | DMA1 Stream 5 |

### ✅ SAI4 (D3 domain) → BDMA + DMAMUX2

**Source:** RM0433 Table 126 (line 113114)

| Peripheral | DMAMUX2 Request | Direction | Configuration |
|------------|-----------------|-----------|---------------|
| SAI4_A | **15** (0x0F) | TX/RX | BDMA Channel (any) |
| SAI4_B | **16** (0x10) | RX | BDMA Channel (any) |

**⚠️ CRITICAL:** SAI4 CANNOT use DMA1 or DMA2. It's in D3 domain and must use BDMA.

### Our Project Configuration

| Peripheral | Use | Controller | Channel/Stream | DMAMUX | Request | Priority | Direction |
|------------|-----|------------|----------------|--------|---------|----------|-----------|
| **SAI1_A** | DAC0 (8ch) | DMA1 | Stream 0 | DMAMUX1 | 87 | High | M2P (TX) |
| **SAI1_B** | DAC1 (8ch) | DMA1 | Stream 5 | DMAMUX1 | 88 | High | M2P (TX) |
| **SAI4_B** | ADC (2ch) | **BDMA** | **Channel 7** | **DMAMUX2** | **16** | Medium | **P2M (RX)** |

---

## DMA1/DMA2 Configuration (SAI1)

### DMA Stream Registers (DMA1/DMA2)

Each stream has these registers:

| Register | Offset | Description |
|----------|--------|-------------|
| DMA_SxCR | 0x00 + 0x18×x | Stream configuration |
| DMA_SxNDTR | 0x04 + 0x18×x | Number of data to transfer |
| DMA_SxPAR | 0x08 + 0x18×x | Peripheral address |
| DMA_SxM0AR | 0x0C + 0x18×x | Memory 0 address (buffer 0) |
| DMA_SxM1AR | 0x10 + 0x18×x | Memory 1 address (buffer 1) |
| DMA_SxFCR | 0x14 + 0x18×x | FIFO control |

Where x = stream number (0-7)

### DMA_SxCR - Stream Configuration Register

**Reset value:** 0x00000000

| Bits | Field | Description | Value for SAI1_A/B |
|------|-------|-------------|---------------------|
| 27:25 | CHSEL[2:0] | ⚠️ DEPRECATED in H7 | Don't use - use DMAMUX |
| 24:23 | MBURST | Memory burst | 00 = single transfer |
| 22:21 | PBURST | Peripheral burst | 00 = single transfer |
| 20 | CT | Current target (read-only) | Toggled by hardware |
| 19 | DBM | Double buffer mode | **1 = enabled** |
| 17:16 | PL[1:0] | Priority | **10 = High** |
| 15 | PINCOS | Peripheral increment offset | 0 |
| 14:13 | MSIZE[1:0] | Memory data size | **10 = 32-bit** |
| 12:11 | PSIZE[1:0] | Peripheral data size | **10 = 32-bit** |
| 10 | MINC | Memory increment | **1 = enabled** |
| 9 | PINC | Peripheral increment | **0 = disabled** |
| 8 | CIRC | Circular mode | **1 = enabled** |
| 7:6 | DIR[1:0] | Transfer direction | **01 = M2P (TX)** |
| 5 | PFCTRL | Peripheral flow controller | 0 = DMA controls |
| 4 | TCIE | Transfer complete interrupt | 1 = enabled |
| 3 | HTIE | Half transfer interrupt | 1 = enabled |
| 2 | TEIE | Transfer error interrupt | 1 = enabled |
| 1 | DMEIE | Direct mode error interrupt | 1 = enabled |
| 0 | EN | Stream enable | 1 = enable (set LAST!) |

### DMAMUX Configuration (for DMA1/DMA2)

**DMAMUX1_CxCR** - Channel Configuration Register

Offset: 0x00 + 0x04×x (where x = channel = stream number)

| Bits | Field | Description | Value |
|------|-------|-------------|-------|
| 31:8 | - | Sync config | 0 (not used for SAI) |
| 7 | SOIE | Sync overrun interrupt | 0 |
| 6:0 | DMAREQ_ID[6:0] | **DMA request ID** | **87** (SAI1_A) or **88** (SAI1_B) |

**Channel mapping:**
- DMA1 Stream 0 → DMAMUX1_Channel0 → Set DMAREQ_ID = 87
- DMA1 Stream 5 → DMAMUX1_Channel5 → Set DMAREQ_ID = 88

### Example: DMA1 Stream 0 for SAI1_A (DAC0)

```c
void DMA1_Stream0_SAI1A_Init(uint32_t *buffer0, uint32_t *buffer1, uint32_t size)
{
    // 1. Enable clocks
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMAMUX1_CLK_ENABLE();

    // 2. Disable stream
    DMA1_Stream0->CR &= ~DMA_SxCR_EN;
    while(DMA1_Stream0->CR & DMA_SxCR_EN);  // Wait

    // 3. Configure DMAMUX1 Channel 0 → SAI1_A request
    DMAMUX1_Channel0->CCR = 87;  // DMAREQ_ID = 87 (SAI1_A)

    // 4. Set addresses
    DMA1_Stream0->PAR = (uint32_t)&(SAI1_Block_A->DR);
    DMA1_Stream0->M0AR = (uint32_t)buffer0;
    DMA1_Stream0->M1AR = (uint32_t)buffer1;
    DMA1_Stream0->NDTR = size;  // Number of data items

    // 5. Configure stream
    DMA1_Stream0->CR = (0 << 23) |   // MBURST = 00
                        (0 << 21) |   // PBURST = 00
                        (1 << 19) |   // DBM = 1 (double buffer)
                        (2 << 16) |   // PL = 10 (high priority)
                        (2 << 13) |   // MSIZE = 10 (32-bit)
                        (2 << 11) |   // PSIZE = 10 (32-bit)
                        (1 << 10) |   // MINC = 1
                        (0 << 9) |    // PINC = 0
                        (1 << 8) |    // CIRC = 1
                        (1 << 6) |    // DIR = 01 (M2P)
                        (1 << 4) |    // TCIE = 1
                        (1 << 3) |    // HTIE = 1
                        (1 << 2) |    // TEIE = 1
                        (1 << 1);     // DMEIE = 1
    // Result: 0x000A6D5E

    // 6. Configure FIFO
    DMA1_Stream0->FCR = (1 << 7) |   // DMDIS = 1 (FIFO enabled)
                         (1 << 6) |   // FEIE = 1
                         (1 << 0);    // FTH = 01 (half full)

    // 7. Enable interrupts
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

    // 8. Enable stream (LAST STEP!)
    DMA1_Stream0->CR |= DMA_SxCR_EN;
}
```

---

## BDMA Configuration (SAI4)

### BDMA Channel Registers

**Source:** RM0433 Chapter 16 (line 105783+)

Each BDMA channel has these registers:

| Register | Offset | Description |
|----------|--------|-------------|
| BDMA_CCRx | 0x08 + 0x14×x | Channel configuration |
| BDMA_CNDTRx | 0x0C + 0x14×x | Number of data to transfer |
| BDMA_CPARx | 0x10 + 0x14×x | Peripheral address |
| BDMA_CM0ARx | 0x14 + 0x14×x | Memory 0 address (buffer 0) |
| BDMA_CM1ARx | 0x18 + 0x14×x | Memory 1 address (buffer 1) |

Where x = channel number (0-7)

**Base address:** BDMA = 0x58025400

### BDMA_CCRx - Channel Configuration Register

**Reset value:** 0x00000000

| Bits | Field | Description | Value for SAI4_B |
|------|-------|-------------|-------------------|
| 31:17 | - | Reserved | - |
| 16 | CT | Current target (read-only) | Toggled by hardware |
| 15 | DBM | Double buffer mode | **1 = enabled** |
| 14 | MEM2MEM | Memory-to-memory | 0 = disabled |
| 13:12 | PL[1:0] | Priority | **01 = Medium** |
| 11:10 | MSIZE[1:0] | Memory size | **10 = 32-bit** |
| 9:8 | PSIZE[1:0] | Peripheral size | **10 = 32-bit** |
| 7 | MINC | Memory increment | **1 = enabled** |
| 6 | PINC | Peripheral increment | **0 = disabled** |
| 5 | CIRC | Circular mode | **1 = enabled** |
| 4 | DIR | Transfer direction | **0 = P2M (RX)** |
| 3 | TEIE | Transfer error interrupt | 1 = enabled |
| 2 | HTIE | Half transfer interrupt | 1 = enabled |
| 1 | TCIE | Transfer complete interrupt | 1 = enabled |
| 0 | EN | Channel enable | 1 = enable (set LAST!) |

**Note:** BDMA is simpler than DMA - no FIFO, no burst transfers.

### DMAMUX2 Configuration (for BDMA)

**DMAMUX2_CxCR** - Channel Configuration Register

Offset: 0x00 + 0x04×x (where x = channel number)

| Bits | Field | Description | Value |
|------|-------|-------------|-------|
| 31:8 | - | Sync config | 0 (not used) |
| 7 | SOIE | Sync overrun interrupt | 0 |
| 6:0 | DMAREQ_ID[6:0] | **DMA request ID** | **16** (SAI4_B) |

**Channel mapping:**
- BDMA Channel 7 → DMAMUX2_Channel7 → Set DMAREQ_ID = 16

### Example: BDMA Channel 7 for SAI4_B (ADC)

```c
void BDMA_Channel7_SAI4B_Init(uint32_t *buffer0, uint32_t *buffer1, uint32_t size)
{
    // 1. Enable clocks
    __HAL_RCC_BDMA_CLK_ENABLE();
    __HAL_RCC_DMAMUX2_CLK_ENABLE();

    // 2. Disable channel
    BDMA_Channel7->CCR &= ~BDMA_CCR_EN;
    while(BDMA_Channel7->CCR & BDMA_CCR_EN);  // Wait

    // 3. Configure DMAMUX2 Channel 7 → SAI4_B request
    DMAMUX2_Channel7->CCR = 16;  // DMAREQ_ID = 16 (SAI4_B)

    // 4. Set addresses
    BDMA_Channel7->CPAR = (uint32_t)&(SAI4_Block_B->DR);
    BDMA_Channel7->CM0AR = (uint32_t)buffer0;
    BDMA_Channel7->CM1AR = (uint32_t)buffer1;
    BDMA_Channel7->CNDTR = size;  // Number of data items

    // 5. Configure channel
    BDMA_Channel7->CCR = (1 << 15) |   // DBM = 1 (double buffer)
                          (1 << 12) |   // PL = 01 (medium priority)
                          (2 << 10) |   // MSIZE = 10 (32-bit)
                          (2 << 8) |    // PSIZE = 10 (32-bit)
                          (1 << 7) |    // MINC = 1
                          (0 << 6) |    // PINC = 0
                          (1 << 5) |    // CIRC = 1
                          (0 << 4) |    // DIR = 0 (P2M, read from peripheral)
                          (1 << 3) |    // TEIE = 1
                          (1 << 2) |    // HTIE = 1
                          (1 << 1);     // TCIE = 1
    // Result: 0x0000DAE

    // 6. Enable interrupts
    HAL_NVIC_SetPriority(BDMA_Channel7_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(BDMA_Channel7_IRQn);

    // 7. Enable channel (LAST STEP!)
    BDMA_Channel7->CCR |= BDMA_CCR_EN;
}
```

### BDMA Interrupt Handler

```c
void BDMA_Channel7_IRQHandler(void)
{
    // Check transfer complete
    if (BDMA->ISR & BDMA_ISR_TCIF7)
    {
        BDMA->IFCR = BDMA_IFCR_CTCIF7;  // Clear flag

        // Check current buffer
        if (BDMA_Channel7->CCR & BDMA_CCR_CT)
        {
            // Using M1AR, process M0AR
            Process_ADC_Buffer0();
        }
        else
        {
            // Using M0AR, process M1AR
            Process_ADC_Buffer1();
        }
    }

    // Check half transfer
    if (BDMA->ISR & BDMA_ISR_HTIF7)
    {
        BDMA->IFCR = BDMA_IFCR_CHTIF7;
        // Half transfer callback (optional)
    }

    // Check transfer error
    if (BDMA->ISR & BDMA_ISR_TEIF7)
    {
        BDMA->IFCR = BDMA_IFCR_CTEIF7;
        // Handle error
    }
}
```

---

## Complete Examples

### Buffer Size Calculation

For 48 kHz, 24-bit audio (stored as 32-bit words):

```c
// TDM8 (8 channels, SAI1_A and SAI1_B):
#define SAMPLE_RATE  48000
#define BUFFER_MS    10
#define SAMPLES_PER_BUFFER  (SAMPLE_RATE * BUFFER_MS / 1000)  // 480 samples

// DAC0 (SAI1_A) - 8 channels
#define SAI1_A_BUFFER_SIZE  (SAMPLES_PER_BUFFER * 8)  // 3840 words
uint32_t sai1_a_buffer0[SAI1_A_BUFFER_SIZE];
uint32_t sai1_a_buffer1[SAI1_A_BUFFER_SIZE];

// DAC1 (SAI1_B) - 8 channels
#define SAI1_B_BUFFER_SIZE  (SAMPLES_PER_BUFFER * 8)  // 3840 words
uint32_t sai1_b_buffer0[SAI1_B_BUFFER_SIZE];
uint32_t sai1_b_buffer1[SAI1_B_BUFFER_SIZE];

// ADC (SAI4_B) - 2 channels
#define SAI4_B_BUFFER_SIZE  (SAMPLES_PER_BUFFER * 2)  // 960 words
uint32_t sai4_b_buffer0[SAI4_B_BUFFER_SIZE];
uint32_t sai4_b_buffer1[SAI4_B_BUFFER_SIZE];
```

### Full Initialization Sequence

```c
void Audio_DMA_Init(void)
{
    // 1. Initialize DMA1 for SAI1_A (DAC0 - 8 channels TX)
    DMA1_Stream0_SAI1A_Init(sai1_a_buffer0, sai1_a_buffer1, SAI1_A_BUFFER_SIZE);

    // 2. Initialize DMA1 for SAI1_B (DAC1 - 8 channels TX)
    DMA1_Stream5_SAI1B_Init(sai1_b_buffer0, sai1_b_buffer1, SAI1_B_BUFFER_SIZE);

    // 3. Initialize BDMA for SAI4_B (ADC - 2 channels RX)
    BDMA_Channel7_SAI4B_Init(sai4_b_buffer0, sai4_b_buffer1, SAI4_B_BUFFER_SIZE);
}
```

---

## Performance Analysis

### Bandwidth Usage @ 48 kHz

| Stream/Channel | Peripheral | Channels | Bandwidth |
|----------------|------------|----------|-----------|
| DMA1 Stream 0 | SAI1_A TX | 8 | 1.536 MB/s |
| DMA1 Stream 5 | SAI1_B TX | 8 | 1.536 MB/s |
| BDMA Channel 7 | SAI4_B RX | 2 | 0.384 MB/s |
| **Total** | - | **18** | **3.456 MB/s** |

Calculation: channels × 32-bit × 48000 Hz = channels × 192000 bytes/sec

**System bandwidth:**
- AXI bus: ~240 MB/s @ 240 MHz
- AHB bus: ~240 MB/s @ 240 MHz
- Audio DMA: 3.5 MB/s (~1.5% of total)

---

## Summary Table

| Peripheral | Controller | Ch/Stream | DMAMUX | Request | Priority | Direction | Channels | Bandwidth |
|------------|------------|-----------|--------|---------|----------|-----------|----------|-----------|
| SAI1_A (DAC0) | DMA1 | Stream 0 | DMAMUX1 | 87 | High | M2P TX | 8 TDM | 1.5 MB/s |
| SAI1_B (DAC1) | DMA1 | Stream 5 | DMAMUX1 | 88 | High | M2P TX | 8 TDM | 1.5 MB/s |
| SAI4_B (ADC) | **BDMA** | **Channel 7** | **DMAMUX2** | **16** | Medium | **P2M RX** | **2 I2S** | **0.4 MB/s** |

---

## References

- **RM0433** STM32H742/H743/H753/H750 Reference Manual
  - Chapter 15: DMA controllers (page 669)
  - Chapter 16: BDMA controller (page 669)
  - Chapter 17: DMAMUX request multiplexer (page 700)
  - Table 102: DMAMUX1 & DMA connections (line 91827)
  - Table 103: DMAMUX2 & BDMA connections (line 92903)
  - Table 122: DMAMUX1 request assignments (line 111894)
  - Table 126: DMAMUX2 request assignments (line 113114)

---

**Document created:** 2026-01-22
**Last verified:** 2026-01-22 against RM0433
**Status:** Verified and corrected
