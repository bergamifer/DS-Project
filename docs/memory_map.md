# STM32H743 Memory Layout

## RAM Distribution
- DTCM: 128 KB (CPU-only — **DMA1/DMA2 NO pueden acceder aquí**)
- AXI SRAM: 512 KB (accesible por CPU y DMA)
- RAM_D2 (SRAM1+SRAM2+SRAM3): 288 KB (accesible por DMA1/DMA2)
- SRAM4: 64 KB (dominio D3, accesible por BDMA)

## Allocation — Confirmado en hardware DS9

- **DMA buffers SAI TX/RX**: RAM_D2 con `__attribute__((aligned(32)))` — crítico
- **Delay lines L/R**: RAM_D2 (1024 muestras × 2 × 4 bytes = 8 KB)
- **Stack y variables locales**: DTCM (más rápido, CPU-only OK)

## Regla crítica

**DMA1/DMA2 NO acceden DTCMRAM** en STM32H743. Cualquier buffer DMA declarado en DTCM
causará silencio/corrupción sin error aparente. Usar sección `.dma_buffer` en el linker
script que mapea a RAM_D2.

```c
// Correcto — en RAM_D2
__attribute__((section(".dma_buffer"), aligned(32)))
int32_t audio_buffer[BUFFER_SIZE];

// Incorrecto — en DTCM (default), DMA no lo ve
int32_t audio_buffer[BUFFER_SIZE];
```