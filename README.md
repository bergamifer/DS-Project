# DS9 Beamforming Audio Processor

Sistema de audio beamforming de 32 canales basado en **STM32H743** + **ESP32-C3-MINI-1**.
Control vía WiFi (HTTP), pipeline DMA circular, latencia ~20ms extremo a extremo.

---

## Hardware Target

| Componente | Detalle |
|---|---|
| MCU | STM32H743VIT6 (LQFP100), Cortex-M7 @ 480 MHz, VOS0 |
| WiFi Bridge | ESP32-C3-MINI-1 |
| DACs | 4× PCM1690 (8 canales TDM cada uno) → 32 canales totales |
| ADC | PCM1802 (2 canales, estéreo) |
| Display | E-Paper SSD1681 200×200 (SPI2) |
| I2C | I2C1 @ PB8/PB9, 100 kHz |
| UART Debug | UART7 @ PA6/PA7 → ST-Link serial |
| UART ESP32 | UART4 @ PA0(TX) / PA1(RX) |

---

## Estado del Firmware (2026-02-19)

| Subsistema | Estado |
|---|---|
| CPU 480 MHz + MCLK 12.288 MHz (PLL3) | ✅ OK |
| 5× SAI DMA circular (32ch TX + 2ch RX) | ✅ 50 callbacks/s, 0 errores, jitter <1 µs |
| 4× PCM1690 I2C init + TDM8 | ✅ Todos responden (0x4C / 0x4D / 0x4E / 0x4F) |
| PCM1802 ADC RX | ✅ DMA corriendo, samples en delay lines |
| Pipeline beamforming 32ch | ✅ Delay / gain / mute por canal, fixed-point |
| ESP32-C3 WiFi + HTTP server | ✅ Conecta, recibe POSTs de configuración |
| UART4 RX (ESP32 → STM32) | ✅ Tramas binarias llegan |
| UART4 TX (STM32 → ESP32) | ⚠️ En investigación (PA0→GPIO0, traza DS9) |

---

## Arquitectura de Audio

```
PCM1802 (ADC)
    │  I2S stereo 24-bit @ 48 kHz
    ▼
SAI3_B RX ─── DMA1_Stream4 ──→ audio_buffer_3b_rx (RAM_D2)
    │
    ▼
delay_line_L[1024] + delay_line_R[1024]   ← circular, 1 sample = ~20.8 µs
    │
    ▼
Fill_Audio_Beamform()   ← per-channel: delay[0-64] / gain[0-255] / mute
    │
    ├──→ SAI1_A + SAI1_B  (ch 0-15)   DMA1_Stream0 / Stream3
    └──→ SAI2_A + SAI2_B  (ch 16-31)  DMA1_Stream1 / Stream2
              │
              ▼
        4× PCM1690 DAC → salidas analógicas
```

**Latencia total:** ~20.6 ms (10 ms DMA input + <0.2 ms compute + 10 ms DMA output)

---

## Configuración SAI (DS10 pinout)

| Bloque | Modo | Pines (AF) | Canales | DMA |
|---|---|---|---|---|
| SAI1_A | Master TX TDM8 | PE4(FS) PE5(SCK) PE6(SD) — AF6 | 0–7 | DMA1_S0 req87 |
| SAI1_B | Slave TX sync | PE3(SD) — AF6 | 8–15 | DMA1_S3 req88 |
| SAI2_A | Master TX TDM8 | PD12(FS) PD13(SCK) PD11(SD) — AF10 | 16–23 | DMA1_S1 req89 |
| SAI2_B | Slave TX sync | PE11(SD) — AF10 | 24–31 | DMA1_S2 req90 |
| SAI3_B | Master RX I2S | PD8(SCK) PD9(SD) PD10(FS) — AF6 | ADC L+R | DMA1_S4 req114 |
| PE2 | MCLK out | SAI1_MCLK_A — AF6 | — | — |

> **DS9 vs DS10:** Las 5 placas DS9 fabricadas tienen 6 errores de pines SAI documentados.
> SAI1_B (ch8-15) y SAI2_A (ch16-23) funcionan en DS9. Ver `docs/ds9_pin_audit.md`.

---

## Clocks

| PLL | Parámetros | Salida |
|---|---|---|
| PLL1 | HSE 25 MHz / M=5 × N=192 / P=2 | **480 MHz** CPU |
| PLL3 | HSE 25 MHz / M=5 × N=68 / P=28, FRACN=6660 | **12.288 MHz** MCLK |

- HCLK = 240 MHz (AHB ÷ 2), APBx = 120 MHz
- FLASH latency: 4 ciclos (requerido para VOS0)

---

## PCM1690 DAC — Direcciones I2C

| DAC | Ref PCB | Addr 7-bit | Addr 8-bit | Canales | SAI |
|---|---|---|---|---|---|
| DAC0 | U3 | 0x4C | 0x98 | 0–7 | SAI1_A |
| DAC1 | U4 | 0x4E | 0x9C | 8–15 | SAI1_B |
| DAC2 | U2 | 0x4D | 0x9A | 16–23 | SAI2_A |
| DAC3 | U8 | 0x4F | 0x9E | 24–31 | SAI2_B |

Init: RST LOW → 1 ms → RST HIGH → 10 ms → I2C TDM8 config.
Req. crítico: MCLK en PE2 debe estar corriendo ANTES de liberar RST (POR del PCM1690).

---

## ESP32-C3 — Protocolo UART

Trama binaria sobre UART @ 115200 8N1 (GPIO1 TX → PA1 RX, GPIO0 RX ← PA0 TX):

```
[0xAA][0x55][CMD][LEN_L][LEN_H][PAYLOAD...][XOR_CHECKSUM][0x0D][0x0A]

CMD 0x01 = UPDATE_CFG (52 bytes payload)
  Byte  0    : master_gain (0–255)
  Byte  1    : mute_global (0/1)
  Bytes 2–33 : delays[16] × uint16 LE  (0–64 samples)
  Bytes 34–49: gains[16]  × uint8      (0–255, 0xFF = unity)
  Bytes 50–51: mutes      × uint16 LE  (bitfield ch0–15)
```

HTTP endpoints del ESP32: `GET/POST /config`, `GET /status`

---

## Memoria

| Región | Uso | Tamaño |
|---|---|---|
| DTCMRAM (128 KB) | Variables, stack | — |
| RAM_D2 (128 KB) | DMA buffers + delay lines | ~120 KB |

Buffers DMA en RAM_D2 con `__attribute__((aligned(32)))`.
DMA1/DMA2 **no acceden DTCMRAM** en STM32H743.

---

## Estructura del Repositorio

```
firmware/
├── Core/Src/
│   ├── main.c              # Pipeline principal: SAI, DMA, beamforming, diagnóstico
│   ├── uart_protocol.c/h   # Parser UART4 — protocolo binario ESP32
│   ├── pcm1690.c/h         # Driver I2C para 4× PCM1690
│   └── epaper.c/h          # Display SSD1681 (opcional)
firmware_esp32c3/
└── main/main.c             # WiFi STA + HTTP server + UART bridge
docs/
├── audio_architecture.md   # Diagrama de arquitectura completa
├── ds9_pin_audit.md        # Errores de pines DS9 y correcciones DS10
└── ds9_pin_map.md          # Mapa de pines DS9
WeAct-AudioTest-CLAUDE.md   # Notas históricas — fase prototipo WeAct
```

---

## Issues Conocidos

| Problema | Severidad | Estado |
|---|---|---|
| PA0→GPIO0 (UART4 TX) sin datos en ESP32 | Alta | En investigación — posible traza DS9 rota |
| DS9: SAI1_A (DAC0) y SAI2_B (DAC3) con pines incorrectos | Media | Documentado, DS10 lo corrige |
| Beamforming usa solo canal L (delay_line_R sin uso) | Media | Pendiente |
| Protocolo ESP32 limitado a 16 canales / mutes 16-bit | Baja | Pendiente expansión a 32ch |

---

## Próximos Pasos

1. Confirmar UART4 RX contando bytes con `RX: N bytes` en status serial
2. Si PA0→GPIO0 no funciona: medir continuidad en PCB DS9
3. Probar audio real en DAC2 (SAI2_A, U2) con osciloscopio o parlante
4. Activar canal R en `Fill_Audio_Beamform()` para beamforming dual
5. Expandir protocolo ESP32 a 32 canales (delays[32], mutes uint32)
6. DS10: corregir asignaciones SAI en PCB (ver `docs/ds9_pin_audit.md`)

---

**Última actualización:** 2026-02-19
