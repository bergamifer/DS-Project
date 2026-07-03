# DS-Project — Beamforming Audio (firmware + hardware)

Sistema de audio beamforming de 32 canales basado en **STM32H743** + **ESP32-C3-MINI-1**.
Control via WiFi (HTTP), pipeline DMA circular, latencia ~20ms extremo a extremo.

> **Repo unificado (2026-07-03):** este repo (antes `DS-Firmware`, ahora **privado**
> como `DS-Project`) incorpora el proyecto KiCad de hardware completo en
> [`hardware/DS/`](hardware/DS/) — historial de commits preservado vía `git subtree`
> desde el repo `DS` original. Un solo lugar para firmware (STM32 + ESP32) y diseño
> de PCB (DS9-DS11).

> **Placa activa: DS10** (en bring-up, junio 2026). DS9 quedó como histórico.
> Estado completo del bring-up: [docs/ds10_bringup_findings.md](docs/ds10_bringup_findings.md).
> Recomendaciones para la placa de producción: [docs/ds11_upgrades.md](docs/ds11_upgrades.md).
> Roadmap de beamforming (Fases 0-4, implementadas en `feature/beamforming`):
> [docs/beamforming_roadmap.md](docs/beamforming_roadmap.md).

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
hardware/DS/                    # Proyecto KiCad completo (historial propio via subtree)
  DS.kicad_pro/.kicad_sch/.kicad_pcb   # Esquemático + PCB activo (DS11 en curso)
  DS11_PLANNING.md              # Plan DS11 + CHECKLIST PRE-JLCPCB UNIFICADO (§8)
  DS10_DESIGN_REVIEW.md, DS10_BRINGUP.md, PowerSpecs.md, ...  # Docs del diseño
  DS.txt                        # Último netlist exportado (referencia para análisis)
firmware/                       # STM32H743 firmware
  Core/Src/main.c               # Pipeline SAI/DMA + beamforming (fases 0-3) + AUTOFOCUS
  Core/Src/uart_protocol.c      # Protocolo binario ESP32<->STM32 (config/modo/beam/status)
  Core/Src/pcm1690.c            # Driver I2C para 4x PCM1690
  Core/Src/eeprom_config.c      # Persistencia de config (I2C4 AF6)
  Core/Src/stm32h7xx_it.c       # IRQ handlers + HardFault dump (UART7, mutea amps)
  Core/Inc/pin_definitions.h    # Fuente de verdad de todos los pines DS10
firmware_esp32c3/               # ESP32-C3 firmware
  main/main.c                   # WiFi + HTTP + UART bridge + UI web embebida (fase 4)
docs/
  reporte_2026-07.md            # REPORTE: camino recorrido y registro de decisiones
  beamforming_roadmap.md        # Fases 0-4 del beamforming (todas implementadas)
  ds10_bringup_findings.md      # Registro completo del bring-up DS10 (8 hallazgos)
  ds11_upgrades.md              # Hallazgos y racional de la revision DS11 (netlist)
  ds10_pin_map.md               # Mapa completo de pines DS10
  audio_architecture.md         # Arquitectura de audio
  ak5538vn_ref.md               # Referencia AK5538VN ADC (config CKS+TDM verificada)
  pcm1690_ref.md                # Referencia PCM1690 DAC
  gpio_control.md               # Control GPIO (DAC RST/MUTE, amps, ADC)
  DS_Hardware_Map_v2.md         # Mapa de hardware (única copia — la de hardware/DS es puntero)
  memory_map.md                 # Layout de RAM del H743 (regla DMA vs DTCM)
  historico/                    # DS9: pin audit, pin map, PCM1802, esquemático PDF
stm32/                          # Referencia RM0433 (clocks, SAI, DMA, I2C, GPIO)
webserver/                      # Control web multi-dispositivo (Flask + SQLite) — ver su README
```

---

## Issues Conocidos

| Problema | Estado |
|---|---|
| Solo 1/4 PCM1690 poblado (rotación JLCPCB) | Reclamo cerrado (USD 355); DACs de repuesto pedidos a Mouser; firmware soporta los 4 |
| ~~Beamforming stub~~ | ✅ Fases 0-4 implementadas en `feature/beamforming` (delay-and-sum RX, TX steering, AUTOFOCUS, control web) — pendiente validación en hardware |
| Skew de fase entre SAI1 y SAI2 (ch0-15 vs 16-31) | Issue #14 — sync por SAI_GCR si hace falta |
| Protocolo ESP32: config de 16ch / mutes 16-bit | `CMD_SET_BEAM`/`CMD_SET_MODE` ya cubren el uso normal; raw de 32ch pendiente (`CMD_RAW_CFG_32`) |
| Underruns residuales (~4) en el bloque verboso de cada 10s | Benignos (DMA reusa half-buffer); recortados de ~15 a ~4 |

---

## Próximos Pasos

1. Inyectar señal al ADC (ZOYI ZT-703S DDS) → verificar captura en ch0
2. Loop completo: `test_tone_enabled=0` → señal inyectada sale por parlantes (ADC→beamform→DAC)
3. Reponer DACs U1/U3/U4 → verificar 32 canales
4. Implementar matriz de beamforming real (issue #13)
5. **Control de volumen/balance per-canal via ESP32** (ver abajo)
6. Aplicar upgrades DS11 para la placa de producción

### Control de volumen via ESP32 (pendiente)

La atenuación de salida vive en los **registros 72-79 del PCM1690** (per-canal,
0.5dB, hoy fijos en −24dB vía `PCM1690_ATTEN_DEFAULT`). Para exponerlo como control
remoto hay que:
1. **Protocolo** (`uart_protocol.h`): nuevo comando `CMD_SET_ATTEN` (o extender el
   payload de config) con un valor de atenuación por canal (0-255 → registro PCM1690).
2. **STM32**: al recibirlo, llamar `PCM1690_SetAttenuation(hi2c1, dac_addr, ch, val)`.
   Nota: es distinto de `beam_config.gains[]` (que se aplica en software en el
   beamformer) — la atenuación del DAC es post-mezcla y no pierde resolución.
3. **ESP32** (`firmware_esp32c3/main/main.c`): pasar el comando nuevo al STM32.
4. **Webserver** (`webserver/app.py` + templates): sliders de volumen/balance por
   canal, persistidos en `ds_config.db`.

---

**Última actualización:** 2026-06-12 — Bring-up DS10 (audio + ADC funcionando)
