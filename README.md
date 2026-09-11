# DS-Project — Beamforming Audio (firmware + hardware)

Sistema de audio beamforming de 32 canales basado en **STM32H743** + **ESP32-C3-MINI-1**.
Control via WiFi (HTTP), pipeline DMA circular, latencia ~20 ms extremo a extremo.

Es el hardware de **Domosónica**, una instalación sonora de ~500 parlantes: cada placa es
un nodo autónomo de 32 canales, y la obra son ~18 nodos en red.

> **Repo unificado (2026-07-03):** este repo (antes `DS-Firmware`, ahora **privado**
> como `DS-Project`) incorpora el proyecto KiCad de hardware completo en
> [`hardware/DS/`](hardware/DS/) — historial de commits preservado vía `git subtree`
> desde el repo `DS` original. Un solo lugar para firmware (STM32 + ESP32) y diseño
> de PCB.

> **¿Primera vez con el equipo?** Empezá por la [guía de uso y conexionado](docs/guia_operacion.md) —
> está escrita en lenguaje llano, sin dar por sabido nada de audio ni de electrónica.
> Y antes de encender amplificadores con micrófonos enchufados, leé
> [qué modos hacen acople](tools/README.md).

---

## Dónde está el proyecto hoy (2026-09-11)

| | Estado |
|---|---|
| **DS11** | **En operación, 3 placas.** 32 canales poblados, audio limpio, soak de 19,6 h |
| **DS12** | **En diseño.** Es la tirada de 10 unidades. Etapa de fuente rediseñada, auditoría cerrada |
| Beamforming | Implementado y **medido**: contraste +17,7 dB dentro/fuera del foco |
| Banco de medición | Dos micrófonos fijos sobre trama TDM común — el foco se verifica con números, no a oído |
| App iOS / sitio público | Andando ([`ios/`](ios/), [`sitio/`](sitio/)) |

El registro de por qué se decidió cada cosa está en los reportes mensuales:
[julio](docs/reporte_2026-07.md) · [agosto](docs/reporte_2026-08.md) · [septiembre](docs/reporte_2026-09.md).

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
| Power | 24V → buck → 5VA → LDOs → 3.3V/3.3VA (acepta 12V de entrada) |

> En DS12 la etapa de fuente cambia entera: el buck pasa a **AP63356** y el LDO digital
> desaparece. El diseño completo, con valores y por qué, está en
> [`hardware/DS/DS12_ALIMENTACION.md`](hardware/DS/DS12_ALIMENTACION.md).

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
Fill_Audio_Beamform()   <- per-channel: delay / gain[0-255] / mute
    |                       (test tone -30dBFS cuando test_tone_enabled=1)
    |---> SAI1_A + SAI1_B  (ch 0-15)   DMA1_Stream0 / Stream3
    '---> SAI2_A + SAI2_B  (ch 16-31)  DMA1_Stream1 / Stream2
              |
              v
        4× PCM1690 DAC --> 32× PAM8302A --> parlantes
```

**Latencia total:** ~20,1 ms (10 ms DMA input + ~0,13 ms compute + 10 ms DMA output)

> **⚠️ Alineamiento SAI:** con `DataSize=24` el SAI transmite bits [23:0] (right-aligned).
> Los samples se escriben directo, **sin `<<8`**. El `<<8` erróneo (heredado de DS9) causaba
> la distorsión "tipo 50 kHz" — ver bring-up findings §1.

---

## SAI — pinout DS10/DS11 (idéntico; DS10 corrigió los errores de DS9)

| Bloque | Modo | Pines (AF) | Canales | DMA |
|---|---|---|---|---|
| SAI1_A | Master TX TDM8 | PE4(FS) PE5(SCK) PE6(SD) — AF6 | 0-7 | DMA1_S0 req87 |
| SAI1_B | Slave TX sync | PE3(SD) — AF6 | 8-15 | DMA1_S3 req88 |
| SAI2_A | Master TX TDM8 | PD12(FS) PD13(SCK) PD11(SD) — AF10 | 16-23 | DMA1_S1 req89 |
| SAI2_B | Slave TX sync | PE11(SD) — AF10 | 24-31 | DMA1_S2 req90 |
| SAI3_B | Master RX TDM256 | PD8(SCK) PD9(SD) PD10(FS) — AF6 | ADC 8ch | DMA1_S4 req114 |
| PE2 | MCLK out (24.576 MHz) | SAI1_MCLK_A — AF6 | — | — |

Mapa completo de pines: [docs/pin_map.md](docs/pin_map.md).

---

## ESP32-C3 — Protocolo UART

Trama binaria @ 115200 8N1 (GPIO0 TX → PA1 RX, GPIO1 RX ← PA0 TX):

```
[0xAA][0x55][CMD][LEN_L][LEN_H][PAYLOAD...][XOR_CHECKSUM][0x0D][0x0A]

CMD 0x01 = UPDATE_CFG (52 bytes payload)
  Byte  0    : master_gain (0-255)
  Byte  1    : mute_global (0/1)
  Bytes 2-33 : delays[16] x uint16 LE
  Bytes 34-49: gains[16]  x uint8      (0-255, 0xFF = unity)
  Bytes 50-51: mutes      x uint16 LE  (bitfield ch0-15)
```

HTTP endpoints: `POST /command` (config JSON), `GET /status`, `GET /ping`

> **Trampa de la API:** `POST /reset` **no reinicia el chip** — es un reset de fábrica, y
> deja la placa en modo 0, que es el del acople. Para reiniciar de verdad: `make ota`.

---

## Build & Flash

```bash
# STM32 (firmware principal) — TARGET = ds11_beamforming
cd firmware && make -j4
make flash-stlink            # st-flash --reset (arranca solo)
make todo-cube               # bootloader + app con CubeProgrammer
make ota OTA_HOST=192.168.100.77   # por WiFi, sin cables

# Debug serial: UART7 (PE7/PE8) → J7 → VCP del ST-Link
screen /dev/cu.usbmodemXXXX 115200

# ESP32-C3
. $HOME/esp/esp-idf/export.sh
cd firmware_esp32c3 && idf.py flash monitor -p /dev/cu.usbmodemYYYY
# o, desde firmware/:  make esp-flash
```

⚠️ **`st-flash` no sirve en todas las placas** — en la segunda y la tercera hay que usar
CubeProgrammer (`make todo-cube`). Ver las notas de cada placa en la memoria del proyecto.

**La app vive en `0x08020000`, no en `0x08000000`**: hay un bootloader propio adelante
(ver [docs/ota_wifi.md](docs/ota_wifi.md)).

---

## Estructura del Repositorio

```
hardware/DS/                    # Proyecto KiCad completo (historial propio via subtree)
  DS.kicad_pro/.kicad_sch/.kicad_pcb   # Esquematico + PCB activo
  DS.kicad_dru                  # Reglas de diseño propias (el QFN de IC3) — ver el README de ahi
  DS12_ALIMENTACION.md          # LA ETAPA DE FUENTE de la DS12: que comprar y como dibujarla
  DS12_PLANNING.md              # Backlog de la DS12 (geometria 2x16, arnes, correcciones)
  DS12_AUDITORIA.md             # Auditoria previa a fabricar: 12 dimensiones, solo lo verificado
  DS12_USB_STM32.md             # USB del STM32 (PA11/PA12 hoy al aire)
  DS11_RELEASE.md               # El KiCad de lo que se fabrico, verificado contra los gerbers
  DS11_PLANNING.md, DS10_*.md   # Historico de revisiones
  jlcpcb/production_files/      # BOM y CPL REALES — la verdad sobre part numbers, no el netlist
  DS.txt                        # Netlist exportado (entrada de tools/netlist_digest.py)
  datasheets/, librerias/, 3d/, plot/, Versiones/

firmware/                       # STM32H743 — make -j4, TARGET=ds11_beamforming
  Core/Src/main.c               # Pipeline SAI/DMA + beamforming + AUTOFOCUS + geometria del array
  Core/Src/uart_protocol.c      # Protocolo binario ESP32<->STM32
  Core/Src/pcm1690.c            # Driver I2C de los 4 DAC
  Core/Src/eeprom_config.c      # Persistencia de config (I2C4 AF6)
  Core/Inc/pin_definitions.h    # Fuente de verdad de todos los pines

firmware_esp32c3/               # ESP32-C3: WiFi + HTTP + puente UART + UI web embebida

tools/                          # Herramientas de host (medicion + hardware) — ver tools/README.md
  check_pcb.sh                  # HOOK: impide commitear gerbers con el cobre desactualizado
  netlist_digest.py             # Convierte DS.txt en tres archivos grepables
  geometria_array.py            # Las 32 posiciones de parlante + DXF del panel, leidas de main.c
  nivel.py abx.py fase.py ...   # Medicion acustica por HTTP (ninguna enciende amps sola)

docs/
  guia_operacion.md             # GUIA DE USO en lenguaje llano
  manual_modos.md               # Los modos de operacion, uno por uno
  reporte_2026-0{7,8,9}.md      # El camino recorrido y por que se decidio cada cosa
  banco_dos_microfonos.md       # El banco de medicion de dos microfonos
  protocolo_medicion.md         # Metodologia acustica (mic fijo, foco movil)
  ds11_errata.md                # Los cortos de fabrica de la DS11 — leer antes de mandar a producir
  ds11_perf.md                  # Performance del pipeline (caches, -O2, soak)
  pin_map.md, memory_map.md     # Pines y layout de RAM (regla DMA vs DTCM)
  ak5538vn_ref.md, pcm1690_ref.md, gpio_control.md, audio_architecture.md
  ota_wifi.md, multinode.md, app_ios.md, instalacion_15_nodos.md
  stm32/                        # Referencia RM0433 (clocks, SAI, DMA, I2C, GPIO)
  datos/                        # Mediciones crudas en JSON + el .md que dice que se medía
  img_osc/, media/              # Capturas de osciloscopio y video
  historico/                    # Bring-up DS10 + esquematico DS9
  pdf/                          # Plantilla y script para generar la guia en PDF

datasheets/                     # Hojas de datos del MCU y los chips de audio
webserver/                      # Control web multi-dispositivo (Flask + SQLite)
sitio/                          # Pagina publica del proyecto
ios/                            # App "Domosonica" (SwiftUI)
mecenazgo/                      # ~320 MB de comprobantes — FUERA de git a proposito
```

**Fuera del repo a propósito:** `firmware/build*/`, `firmware_esp32c3/build/`,
`webserver/venv/`, `mecenazgo/`, `.ds12_audit/` (derivado del netlist), `*.wav`,
`docs/pdf/*.pdf` (se regenera con `docs/pdf/hacer_pdf.sh`), y `secrets.h`.

---

## Antes de mandar una placa a producir

```bash
tools/check_pcb.sh --install    # una sola vez por copia del repo
```

Instala un hook de pre-commit que corre el DRC dos veces —con los rellenos guardados y
con `--refill-zones`— y **bloquea el commit si el cobre guardado está viejo**. KiCad no
recalcula los rellenos al plotear: usa lo que está guardado. Ese mecanismo ya arruinó dos
tandas de la DS11 ([docs/ds11_errata.md](docs/ds11_errata.md)) y apareció una tercera vez,
latente, en septiembre.

Al exportar los gerbers, además: `kicad-cli pcb export gerbers --check-zones` (viene
apagado por default).

---

## Issues Conocidos

| Problema | Estado |
|---|---|
| Skew de fase entre SAI1 y SAI2 (ch0-15 vs 16-31) | Medido: **4 µs** entre bancos. Sync por SAI_GCR si algún día molesta |
| Protocolo ESP32: config de 16ch / mutes 16-bit | `CMD_SET_BEAM`/`CMD_SET_MODE` cubren el uso normal; raw de 32ch pendiente |
| Underruns residuales (~4) en el bloque verboso de cada 10 s | Benignos (el DMA reusa el half-buffer) |
| PA11/PA12 (USB_DM/DP del STM32) al aire | Ver [DS12_USB_STM32.md](hardware/DS/DS12_USB_STM32.md) |
| Prueba de escucha del `-O2` | Pendiente desde la mejora de 43× del pipeline |

---

## Próximos Pasos

1. **Cerrar la DS12 y mandar a producir las 10 placas** — etapa de fuente + los ítems de
   esquemático del §3 de [DS12_AUDITORIA.md](hardware/DS/DS12_AUDITORIA.md)
2. Cortar y armar los nodos con la geometría **2×16** (el firmware ya la soporta:
   `SPK_LAYOUT_2X16` en `main.c`)
3. Control de volumen/balance per-canal vía ESP32 (registros 72-79 del PCM1690, hoy fijos
   en −24 dB por `PCM1690_ATTEN_DEFAULT`)
4. Instalación multi-nodo: [docs/multinode.md](docs/multinode.md) y
   [docs/instalacion_15_nodos.md](docs/instalacion_15_nodos.md)

---

**Última actualización:** 2026-09-11 — auditoría DS12 aplicada, orden del repo, hook de PCB.
