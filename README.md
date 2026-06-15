# DS10 Beamforming Audio Processor

Sistema de audio beamforming de 32 canales basado en **STM32H743** + **ESP32-C3-MINI-1**.
Control via WiFi (HTTP), pipeline DMA circular, latencia ~20ms extremo a extremo.

> **Placa activa: DS10** (en bring-up, junio 2026). DS9 quedó como histórico.
> Estado completo del bring-up: [docs/ds10_bringup_findings.md](docs/ds10_bringup_findings.md).
> Recomendaciones para la placa de producción: [docs/ds11_upgrades.md](docs/ds11_upgrades.md).

---

## Hardware

| Componente | Detalle |
|---|---|
| MCU | STM32H743VIT6 (LQFP100), Cortex-M7 @ 480 MHz, VOS0 |
| WiFi Bridge | ESP32-C3-MINI-1 (UART4 @ 115200, PA0/PA1) |
| DACs | 4× PCM1690 (8ch TDM8 c/u) = 32 canales — I2C1: 0x98, 0x9C, 0x9A, 0x9E |
| ADC | AK5538VN (8ch TDM256) via SAI3_B — I2C3 addr 0x22 (7-bit 0x11) |
| Amps | 32× PAM8302A Class-D 2.5W (shutdown compartido PD3) |
| EEPROM | CAT24C256 (I2C4 PB6/PB7 **AF6**, WP=PB5) |
| Power | 24V → MP2315S buck → 5VA → LDOs → 3.3V/3.3VA (acepta 12V de entrada) |

---

## Estado (2026-06) — Bring-up DS10 en hardware

| Subsistema | Estado |
|---|---|
| CPU 480 MHz + MCLK 24.576 MHz | ✅ VOS0, PLL1/PLL3 |
| 5× SAI DMA circular (32ch TX + 8ch RX) | ✅ 50 half + 50 full/s, 0 errores, jitter ~0 |
| PCM1690 DAC0 (U2) TDM8 + audio | ✅ **senoidal limpia verificada en parlantes** |
| AK5538VN ADC 8ch TDM256 | ✅ **capturando** (TDM configurado por I2C) |
| EEPROM config persistence | ✅ load/save (I2C4 AF6) |
| Beamforming 32ch (delay/gain/mute) | ✅ Fill ~127µs, pipeline armado |
| ESP32 WiFi + UART link | ✅ firmware listo, sin contención con debug (UART7) |
| HardFault diagnostics | ✅ dump PC/LR/HFSR/CFSR via UART7 + mutea amps |

> **Limitación actual del hardware:** solo 1 de 4 PCM1690 poblado (U1/U3/U4 vinieron
> rotados 180° de JLCPCB → desoldados, reclamo en curso). El firmware soporta los 32
> canales; al reponer los DACs funcionan sin cambios. Ver bring-up findings §7.

---

## Arquitectura de Audio

```
AK5538VN (ADC 8ch)
    |  TDM256 32-bit @ 48 kHz (config TDM por I2C3, addr 0x22)
    v
SAI3_B RX (Master) --- DMA1_Stream4 --> audio_buffer_3b_rx (RAM_D2)
    |
    v
delay_line[8][1024]   <- circular, 8 canales
    |  Process_ADC_Half() deinterleave + (>>8 a 24-bit)
    v
Fill_Audio_Beamform()   <- per-channel: delay[0-64] / gain[0-255] / mute
    |                       (test tone -30dBFS cuando test_tone_enabled=1)
    |---> SAI1_A + SAI1_B  (ch 0-15)   DMA1_Stream0 / Stream3
    '---> SAI2_A + SAI2_B  (ch 16-31)  DMA1_Stream1 / Stream2
              |
              v
        4× PCM1690 DAC --> 32× PAM8302A --> parlantes
```

**Latencia total:** ~20.1 ms (10 ms DMA input + ~0.13 ms compute + 10 ms DMA output)

> **⚠️ Alineamiento SAI:** con `DataSize=24` el SAI transmite bits [23:0] (right-aligned).
> Los samples se escriben directo, **sin `<<8`**. El `<<8` erróneo (heredado de DS9) causaba
> la distorsión "tipo 50kHz" — ver bring-up findings §1.

---

## SAI — DS10 pinout (sin workarounds; DS10 corrige los errores de DS9)

| Bloque | Modo | Pines (AF) | Canales | DMA |
|---|---|---|---|---|
| SAI1_A | Master TX TDM8 | PE4(FS) PE5(SCK) PE6(SD) — AF6 | 0-7 | DMA1_S0 req87 |
| SAI1_B | Slave TX sync | PE3(SD) — AF6 | 8-15 | DMA1_S3 req88 |
| SAI2_A | Master TX TDM8 | PD12(FS) PD13(SCK) PD11(SD) — AF10 | 16-23 | DMA1_S1 req89 |
| SAI2_B | Slave TX sync | PE11(SD) — AF10 | 24-31 | DMA1_S2 req90 |
| SAI3_B | Master RX TDM256 | PD8(SCK) PD9(SD) PD10(FS) — AF6 | ADC 8ch | DMA1_S4 req114 |
| PE2 | MCLK out (24.576 MHz) | SAI1_MCLK_A — AF6 | — | — |

Mapa completo de pines: [docs/ds10_pin_map.md](docs/ds10_pin_map.md).

---

## ESP32-C3 — Protocolo UART

Trama binaria @ 115200 8N1 (GPIO0 TX → PA1 RX, GPIO1 RX ← PA0 TX):

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

## Build & Flash

```bash
# STM32 (firmware principal)
cd firmware && make -j4
make flash-stlink            # st-flash --reset (arranca solo)

# Debug serial: UART7 (PE7/PE8) → J7 → VCP del ST-Link
screen /dev/cu.usbmodemXXXX 115200
# (~/.screenrc con 'termcapinfo xterm* ti@:te@' para scroll nativo)

# ESP32-C3
. $HOME/esp/esp-idf/export.sh
cd firmware_esp32c3
idf.py flash -p /dev/cu.usbmodemYYYY && idf.py monitor -p /dev/cu.usbmodemYYYY
```

---

## Estructura del Repositorio

```
firmware/                       # STM32H743 firmware
  Core/Src/main.c               # Pipeline: SAI, DMA, beamforming, diagnostico de boot
  Core/Src/uart_protocol.c      # Parser binario ESP32->STM32 + recovery de overrun
  Core/Src/pcm1690.c            # Driver I2C para 4x PCM1690
  Core/Src/eeprom_config.c      # Persistencia de config (I2C4 AF6)
  Core/Src/stm32h7xx_it.c       # IRQ handlers + HardFault dump (UART7, mutea amps)
  Core/Inc/pin_definitions.h    # Fuente de verdad de todos los pines DS10
firmware_esp32c3/               # ESP32-C3 firmware
  main/main.c                   # WiFi STA + HTTP server + UART bridge
docs/
  ds10_bringup_findings.md      # Registro completo del bring-up (7 hallazgos)
  ds11_upgrades.md              # Recomendaciones para la placa de produccion
  ds10_pin_map.md               # Mapa completo de pines DS10
  audio_architecture.md         # Arquitectura de audio v4.0
  ak5538vn_ref.md               # Referencia AK5538VN ADC (config TDM verificada)
  pcm1690_ref.md                # Referencia PCM1690 DAC
  gpio_control.md               # Control GPIO (DAC RST/MUTE, amps, ADC)
  DS_Hardware_Map_v2.md         # Mapa de hardware (DS10 + histórico DS9)
  ds9_pin_audit.md, ds9_pin_map.md, pcm1802_registers.md   # Histórico DS9
stm32/                          # Referencia RM0433 (clocks, SAI, DMA, I2C, GPIO)
webserver/                      # Control web (Flask + SQLite)
```

---

## Issues Conocidos

| Problema | Estado |
|---|---|
| Solo 1/4 PCM1690 poblado (rotación JLCPCB) | Reclamo en curso; firmware soporta los 4 |
| Beamforming usa mapeo stub (ch8-31 leen mic0) | Issue #13 — implementar matriz real |
| Skew de fase entre SAI1 y SAI2 (ch0-15 vs 16-31) | Issue #14 — sync por SAI_GCR si hace falta |
| Protocolo ESP32 limitado a 16ch / mutes 16-bit | Pendiente expansión a 32ch |
| Underruns en bursts de print verbose | Benignos (DMA reusa half-buffer) |

---

## Próximos Pasos

1. Inyectar señal al ADC (ZOYI ZT-703S DDS) → verificar captura en ch0
2. Loop completo: `test_tone_enabled=0` → señal inyectada sale por parlantes (ADC→beamform→DAC)
3. Reponer DACs U1/U3/U4 → verificar 32 canales
4. Implementar matriz de beamforming real (issue #13)
5. Aplicar upgrades DS11 para la placa de producción

---

**Última actualización:** 2026-06-12 — Bring-up DS10 (audio + ADC funcionando)
