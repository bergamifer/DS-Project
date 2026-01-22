# STM32H743 Memory Layout

## RAM Distribution
- DTCM: 128 KB (fastest, para buffers críticos)
- AXI SRAM: 512 KB (DMA buffers)
- SRAM1: 128 KB
- SRAM2: 128 KB
- SRAM3: 32 KB
- SRAM4: 64 KB

## Allocation Plan
- Delay buffers: SRAM1/2 (307 KB)
- DMA buffers: AXI SRAM (18 KB)
- Stack: DTCM