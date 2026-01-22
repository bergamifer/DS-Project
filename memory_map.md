# STM32H743 Memory Layout

## RAM Distribution
- DTCM: 128 KB (fastest, para buffers críticos)
- AXI SRAM: 512 KB (DMA buffers)
- SRAM1: 128 KB
- SRAM2: 128 KB
- SRAM3: 32 KB
- SRAM4: 64 KB

## Allocation Plan
- Delay buffers: SRAM1/2 (256 KB para 16 canales × 16KB cada uno, delays hasta 100ms)
- DMA buffers: AXI SRAM (por definir según tamaño de buffer circular)
- Stack y variables críticas: DTCM (128 KB - acceso más rápido)