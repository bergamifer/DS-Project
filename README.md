# DS9 Beamforming Audio Processor

Sistema de audio beamforming de 32 canales basado en **STM32H743** + **ESP32-C3-MINI-1**.
Control via WiFi (HTTP), pipeline DMA circular, latencia ~20ms extremo a extremo.

---

## Hardware

| Componente | Detalle |
|---|---|
| MCU | STM32H743VIT6 (LQFP100), Cortex-M7 @ 480 MHz, VOS0 |
| WiFi Bridge | ESP32-C3-MINI-1 (UART4 @ 115200) |
| DACs | 4x PCM1690 (8ch TDM c/u) = 32 canales totales |
| ADC | PCM1802 (2ch estereo) via SAI3_B |
| Display | E-Paper SSD1681 200x200 (SPI2, opcional) |
| I2C | I2C1 @ PB8/PB9 — 4x PCM1690: 0x98, 0x9A, 0x9C, 0x9E |

---

## Estado (2026-02-20) — Confirmado en hardware DS9

| Subsistema | Estado |
|---|---|
| CPU 480 MHz + MCLK 12.288 MHz | ✅ VOS0, PLL1/PLL3 |
| 5x SAI DMA circular (32ch TX + 2ch RX) | ✅ 50 half + 50 full callbacks/s, 0 errores |
| 4x PCM1690 I2C init + TDM8 | ✅ 4/4 responden |
| Beamforming 32ch (delay/gain/mute) | ✅ Fill ~909 us (9.1% CPU), jitter 0 us |
| ESP32 WiFi + HTTP server | ✅ Conecta, POST /command funciona |
| UART4 bidireccional (STM32 ↔ ESP32) | ✅ TX+RX confirmado, "Config applied" |
| HardFault diagnostics | ✅ Dump PC/LR/HFSR/CFSR via UART4 directo |

---

## Arquitectura de Audio

```
PCM1802 (ADC)
    |  I2S stereo 24-bit @ 48 kHz
    v
SAI3_B RX --- DMA1_Stream4 --> audio_buffer_3b_rx (RAM_D2)
    |
    v
delay_line_L[1024] + delay_line_R[1024]   <- circular
    |
    v
Fill_Audio_Beamform()   <- per-channel: delay[0-64] / gain[0-255] / mute
    |
    |---> SAI1_A + SAI1_B  (ch 0-15)   DMA1_Stream0 / Stream3
    '---> SAI2_A + SAI2_B  (ch 16-31)  DMA1_Stream1 / Stream2
              |
              v
        4x PCM1690 DAC --> salidas analogicas
```

**Latencia total:** ~20.6 ms (10 ms DMA input + <0.2 ms compute + 10 ms DMA output)

---

## SAI (DS9 pinout con workarounds)

| Bloque | Modo | Pines (AF) | Canales | DMA |
|---|---|---|---|---|
| SAI1_A | Master TX TDM8 | PE4(FS) PE5(SCK) PE6(SD) -- AF6 | 0-7 | DMA1_S0 req87 |
| SAI1_B | Slave TX sync | PE3(SD) -- AF6 | 8-15 | DMA1_S3 req88 |
| SAI2_A | Master TX TDM8 | PD12(FS) PD13(SCK) PD11(SD) -- AF10 | 16-23 | DMA1_S1 req89 |
| SAI2_B | Slave TX sync | PE11(SD) -- AF10 | 24-31 | DMA1_S2 req90 |
| SAI3_B | Master RX I2S | PD8(SCK) PD9(SD) PD10(FS) -- AF6 | ADC L+R | DMA1_S4 req114 |
| PE2 | MCLK out | SAI1_MCLK_A -- AF6 | -- | -- |

> **DS9 pin conflicts:** PE3 compartido con DAC1_RST, PE11 con DAC0_AMUTEI.
> Workaround: init DAC primero, luego reconfigurar pin a SAI AF.
> Ver `docs/ds9_pin_audit.md` para los 6 errores de pines. DS10 los corrige.

---

## ESP32-C3 -- Protocolo UART

Trama binaria @ 115200 8N1 (GPIO0 TX -> PA1 RX, GPIO1 RX <- PA0 TX):

```
[0xAA][0x55][CMD][LEN_L][LEN_H][PAYLOAD...][XOR_CHECKSUM][0x0D][0x0A]

CMD 0x01 = UPDATE_CFG (52 bytes payload)
  Byte  0    : master_gain (0-255)
  Byte  1    : mute_global (0/1)
  Bytes 2-33 : delays[16] x uint16 LE  (0-64 samples)
  Bytes 34-49: gains[16]  x uint8      (0-255, 0xFF = unity)
  Bytes 50-51: mutes      x uint16 LE  (bitfield ch0-15)
```

HTTP endpoints: `POST /command` (config JSON), `GET /status`, `GET /ping`

---

## Estructura del Repositorio

```
firmware/                       # STM32H743 firmware
  Core/Src/main.c               # Pipeline: SAI, DMA, beamforming, diagnostico
  Core/Src/uart_protocol.c      # Parser binario ESP32->STM32
  Core/Src/pcm1690.c            # Driver I2C para 4x PCM1690
  Core/Src/stm32h7xx_it.c       # IRQ handlers + HardFault dump
  Core/Src/epaper.c             # Display SSD1681 (opcional)
firmware_esp32c3/               # ESP32-C3 firmware
  main/main.c                   # WiFi STA + HTTP server + UART bridge
docs/                           # Documentacion tecnica
  audio_architecture.md         # Diagrama de audio
  ds9_pin_audit.md              # Errores de pines DS9 y workarounds
  ds9_pin_map.md                # Mapa completo de pines DS9
  pcm1690_ref.md                # Referencia PCM1690 DAC
  pcm1802_registers.md          # Referencia PCM1802 ADC
stm32/                          # Referencia RM0433 (clocks, SAI, DMA, I2C, GPIO)
webserver/                      # Control web (Flask + SQLite)
```

---

## Issues Conocidos

| Problema | Estado |
|---|---|
| STLink serial contiene PA0/PA1 con ESP32 | Desconectar STLink serial antes de usar ESP32 |
| DS9: SAI1_A (DAC0) y SAI2_B (DAC3) pines incorrectos | Documentado, DS10 lo corrige |
| Beamforming usa solo canal L (delay_line_R sin uso) | Pendiente |
| Protocolo ESP32 limitado a 16ch / mutes 16-bit | Pendiente expansion a 32ch |

---

## Proximos Pasos

1. Probar audio real en DAC2 (SAI2_A, U2) con osciloscopio o parlante
2. Activar canal R en `Fill_Audio_Beamform()` para beamforming dual
3. Expandir protocolo ESP32 a 32 canales (delays[32], mutes uint32)
4. DS10: corregir asignaciones SAI en PCB

---

**Ultima actualizacion:** 2026-02-20
