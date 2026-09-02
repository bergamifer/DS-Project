# Dónde vive cada cosa en la memoria del STM32H743

Verificado contra `firmware/Core/Src/main.c` y `firmware/STM32H743XX_FLASH.ld`
el 2026-09-01. El firmware además lo chequea en cada arranque
(`Check_Memory_Placement()`) e imprime `Memoria: … — OK` en el banner.

## Las cuatro RAM, y para qué sirve cada una

| Región | Tamaño | Quién la alcanza | Qué guardamos |
|---|---|---|---|
| **DTCM** `0x20000000` | 128 KB | **Sólo el CPU** — DMA1/DMA2 **no** | stack, variables, y las **delay lines** |
| **AXI SRAM** `0x24000000` | 512 KB | CPU y DMA | el buffer de captura del ADC |
| **RAM_D2** `0x30000000` | 288 KB | DMA1/DMA2 | **todos los buffers de DMA de audio** |
| SRAM4 `0x38000000` | 64 KB | dominio D3 / BDMA | sin usar |

## La regla que hay que saber

> **DMA1 y DMA2 no acceden a DTCM.** Un buffer de DMA declarado ahí produce
> silencio o datos corruptos **sin ningún error**: el periférico arranca, los
> contadores avanzan, y no sale audio. Es de los fallos más caros de diagnosticar
> porque todo lo que uno mira dice que está bien.

Por eso los buffers de DMA llevan sección explícita:

```c
/* Correcto — RAM_D2, alcanzable por DMA */
static int32_t audio_buffer[AUDIO_BUFFER_SIZE * 2]
    __attribute__((section(".dma_buffer"), aligned(32)));

/* Incorrecto — DTCM (el default), el DMA no lo ve */
static int32_t audio_buffer[AUDIO_BUFFER_SIZE * 2];
```

El `aligned(32)` es por la línea de caché: no hace falta hoy (la D-cache está
apagada) y se deja porque el día que se encienda, la alineación es la diferencia
entre funcionar y corromperse en los bordes.

## Qué hay hoy en cada lado

**RAM_D2 — `.dma_buffer`**, los cinco buffers dobles de los SAI:

| Buffer | Destino | Tamaño |
|---|---|---|
| `audio_buffer` | SAI1_A → DAC banco 0 (ch 0-7) | 240 × 8 × 4 × 2 = 15,4 KB |
| `audio_buffer_1b` | SAI1_B → DAC banco 8 | 15,4 KB |
| `audio_buffer_2a` | SAI2_A → DAC banco 16 | 15,4 KB |
| `audio_buffer_2b` | SAI2_B → DAC banco 24 | 15,4 KB |
| `audio_buffer_3b_rx` | SAI3_B ← AK5538VN, 8 entradas | 15,4 KB |

**DTCM — las delay lines.** `delay_line[8][1024]` de `int32_t` = **32 KB**.

⚠️ Esto cambió el 2026-08-13 y es la corrección más importante de este documento:
antes estaban en RAM_D2 (llegaban ahí por `.dma_buffer`) y **no son memoria de
DMA** — las escribe el CPU en `Process_ADC_Half` copiando desde
`audio_buffer_3b_rx`. Es la estructura más leída del pipeline: el modo 15 hace 8
lecturas por muestra de salida **y por canal**. Moverlas a DTCM —0 wait states,
acoplada al core— fue parte del cambio que hizo entrar el modo 15 en presupuesto
de CPU. Ver `ds11_perf.md`.

`beam_line[1024]` (4 KB) vive con ellas, por el mismo motivo.

**AXI SRAM — `.axi_ram`.** `cap_buf[8192][8]` de `int32_t` = **256 KB**, el buffer
de la captura remota del ADC (`GET /capture`). Estuvo entera sin usar hasta el
2026-08-23: 256 KB acá no le sacan nada a nadie, mientras que en DTCM no entrarían
y en RAM_D2 competirían con los buffers de DMA.

## Presupuesto

```
text 91.352   data 332   bss 452.848      (2026-09-01)
```

El `bss` incluye los 256 KB de `cap_buf` en AXI SRAM, así que **no** son todos
DTCM. De los 128 KB de DTCM, las delay lines más `beam_line` se llevan 36 KB.

## Flash

El firmware **no** arranca en `0x08000000`: ahí vive el bootloader. La aplicación
está en **`0x08020000`** (banco 1), y las actualizaciones por WiFi se escriben en
el banco 2 sin tocar la imagen que está corriendo. Ver `ota_wifi.md`.
