# Manual de modos de operación — DS11 (firmware STM32)

> Fuente: `firmware/Core/Src/main.c`, tabla `ui_states[]` (16 modos, índices 0–15).
> Última actualización: 2026-07-12.

## Cómo se cambia de modo

| Vía | Mecanismo |
|-----|-----------|
| **Botón K2** (PA4, presionar = LOW) | Avanza un modo por pulsación, con wrap 15→0. Debounce de 30 ms. |
| **HTTP** (web/app vía ESP32) | `POST /mode` con `{"mode":N}` → trama `CMD_SET_MODE` por UART4. |
| **UART4 directo** | Trama binaria `CMD_SET_MODE` (0x02), payload = 1 byte con el índice. |

Cada cambio imprime en UART7: `MODE [n/16]: <nombre>`.

**Persistencia**: el último modo se guarda en EEPROM con debounce de 3 s
(`CFG_SAVE_DEBOUNCE_MS`, compartido con volumen y config) — ciclar rápido con
K2 o arrastrar un slider no gasta escrituras ni mete ráfagas de underruns;
solo se persiste el estado en el que te quedaste. Al boot se restaura el último modo,
**excepto AUTOFOCUS** (modo 11): dispararía un scan con chirps sin el micrófono
puesto, así que cae a modo 0 y la EEPROM se reescribe con 0.

**Regla de exclusión**: `UI_ApplyState()` garantiza que exactamente un modo de
salida está activo (flags `monitor` / `walker` / `beam` / `af`). Al entrar a un
modo beam desde otro que no lo era, `beam_line` se limpia a cero.

## Tabla resumen

| # | Nombre (UART) | Fuente | Salida | Notas |
|---|---------------|--------|--------|-------|
| 0 | `ADC monitor (+30dB)` | ADC ch0–7 en vivo | Las 8 entradas repetidas en cada fila | Boost ×32, modo por defecto |
| 1 | `Channel walker (1s/ch)` | Tono 1 kHz interno | Un canal por vez, 1 s cada uno | **32 canales** = 32 s por vuelta |
| 2 | `RX beam az=0 (broadside)` | Beam delay-and-sum de los 8 ADC | Beam mono en todos los canales | Boost ×32 |
| 3 | `RX beam az=+30` | ídem, steering +30° | ídem | |
| 4 | `RX beam az=-30` | ídem, −30° | ídem | |
| 5 | `RX beam az=+60` | ídem, +60° | ídem | |
| 6 | `RX beam az=-60` | ídem, −60° | ídem | |
| 7 | `TX focus center @1m` | RX beam broadside | 32 canales con delays focales | Foco (0, 0, 1000) mm |
| 8 | `TX focus left 0.5m @1m` | ídem | ídem | Foco (−500, 0, 1000) mm |
| 9 | `TX focus right 0.5m @1m` | ídem | ídem | Foco (+500, 0, 1000) mm |
| 10 | `TX focus center NEAR 0.3m` | ídem | ídem | Foco (0, 0, 300) mm |
| 11 | `AUTOFOCUS scan (mic->ADC c0)` | Chirp por parlante + mic | Scan → tono enfocado medido | Requiere mic en ADC c0 |
| 12 | `TX tone 2k focus @1m` | Tono interno 2 kHz | TX steering completo | Valida direccionalidad sin mic |
| 13 | `TX tone 1k focus @1m` | Tono interno 1 kHz | ídem | Spot ~2× más ancho que el 12 |
| 14 | `MULTIFOCUS 4 tonos @1m` | 4 tonos internos (1.3/1.6/1.9/2.1 kHz) | Cada tono enfocado en su propio punto | Precuela de la intervención 8-in |
| 15 | `MULTIFOCUS 8 entradas @1m` | Las 8 entradas analógicas | Cada entrada enfocada en su propio punto | **La intervención**: 8 señales, 8 lugares |

**IMPORTANTE (append-only)**: los índices están espejados en el ESP32, la web y
la app iOS. Modos nuevos se AGREGAN al final; nunca reordenar.

---

## Modo 0 — ADC monitor (+30 dB)

Ruta directa ADC→DAC para *escuchar* lo que captura el AK5538. Cada canal de
salida reproduce su canal ADC homólogo (DAC ch0 ← ADC ch0, etc.). En DS11 las
8 entradas se **repiten en cada fila** de 8 parlantes: la entrada N suena en la
columna N de las 4 filas. La señal del ADC es débil (hum ~−55 dBFS),
así que se aplica `adc_monitor_shift = 5` (×32 ≈ +30 dB) y clamp a ±FS 24-bit.

- **Uso**: verificar cadena de entrada, mapear entrada física → slot TDM
  (junto con la línea `ADCpk` del heartbeat).
- Es el modo por defecto de primer boot.

## Modo 1 — Channel walker (1 s/canal)

Tono de 1 kHz a −6 dBFS en **un solo canal de salida por vez**, avanzando cada
`WALKER_SECONDS_PER_CH` (1 s). Recorre `WALKER_NUM_CH` canales — **32 en DS11**
(los 4 PCM1690 poblados): una vuelta completa dura 32 s y valida el mapa
canal→parlante y el cableado JSPK, incluida la doble inversión deliberada de
los canales impares (al JST llega la polaridad correcta — ver
`docs/ds11_upgrades.md`).

- UART7 imprime en cada avance: `WALKER: chN (DACd slotS)`.
- **Uso**: validar mapeo canal→parlante y, con osciloscopio o mic, la polaridad.
- El volumen maestro (`POST /volume`) también afecta este modo.

## Modos 2–6 — RX beam (delay-and-sum, presets de azimut)

Beamforming **de recepción** sobre los 8 canales del ADC. `Beam_UpdateDelays()`
calcula delays far-field por micrófono: τ_m = x_m·sin(az)/c, normalizados a
mínimo 0, redondeados a muestras @48 kHz (tope `MAX_DELAY_SAMPLES = 64`).

- Geometría de mics: **placeholder** — array lineal en X, pitch 50 mm
  (`mic_pos_x_mm[]`). Editar esa tabla cuando se defina la geometría real.
- La suma de los 8 canales se escala (`>>8` por término y `>>3` al total) para
  que señal coherente idéntica en los 8 mics salga ~unity, y se escribe en
  `beam_line[]`.
- Salida: el beam mono en **todos** los canales de salida, con boost ×32
  (`beam_out_shift = 5`) — la fuente ADC es débil.
- Azimut: 0 = broadside; positivo hacia índices de mic más altos.
- El heartbeat agrega la línea `BEAM az=<az> pk=<pico>`: con la misma señal
  inyectada en todos los mics, steering "hacia la fuente" ≈ ADCpk de un canal;
  steering lejos → cae. Esa diferencia es la validación del delay-and-sum.
- **Override experimental**: si llega `CMD_UPDATE_CFG` estando en modo beam,
  `delays[0..7]` del config se aplican como delays RX crudos por mic
  (pisan la tabla calculada hasta el próximo cambio de modo).

## Modos 7–10 — TX focus (steering de transmisión, presets)

Beamforming **de transmisión** near-field: la fuente es el RX beam a broadside
(az = 0), y cada canal de salida la reproduce retrasada para que los frentes de
onda converjan en el punto focal.

- `Beam_UpdateTxFocus()`: d_k = |foco − parlante_k|/c; delay_k = max(d) − d_k
  (el parlante más lejano dispara primero). Delays en **Q8** (1 LSB = 1/256 de
  muestra ≈ 0.03 mm de camino acústico), interpolación lineal entre muestras
  adyacentes de `beam_line`. Tope `TX_MAX_DELAY_SAMPLES = 128`.
- Geometría de parlantes: 4×8 patrón ladrillo (`Beam_InitTxGeometry`), pitch
  X 80 mm / Y 160 mm, filas 2 y 4 corridas +40 mm. Mapeo ch = fila·8 + col
  (validar con el walker sobre la DS11 ya poblada).
- Marco de referencia: el del OYENTE mirando los conos. +X = derecha del
  oyente, +Y = hacia la fila 4, +Z = hacia el oyente. Coordenadas del foco
  relativas al centro del array.
- Se aplican por canal: gain (`beam_config.gains`), mute, master gain/mute
  global y `speaker_sign` (polaridad, +1 por defecto; el AUTOFOCUS la mide).
- Boost ×32 igual que los modos RX (fuente ADC débil).
- `tx_delay_q8[]` es deliberadamente **independiente** de
  `beam_config.delays[]`: un config del ESP32 no pisa el foco.

| Modo | Foco (fx, fy, fz) mm | Qué valida |
|------|---------------------|------------|
| 7 | (0, 0, 1000) | Foco centrado a 1 m |
| 8 | (−500, 0, 1000) | Foco 0.5 m a la izquierda |
| 9 | (+500, 0, 1000) | Foco 0.5 m a la derecha |
| 10 | (0, 0, 300) | Campo cercano, 30 cm |

Al entrar, UART7 imprime la tabla de delays por fila (`samples.frac`) para
sanity-check geométrico.

## Modo 11 — AUTOFOCUS scan (mic → ADC c0)

Calibración focal **medida** (no modelada): absorbe error de geometría, de
mapeo de canales y polaridad invertida de parlantes.

**Preparación**: colocar un micrófono EN el punto focal deseado, conectado a
la entrada 1 del ADC (canal c0). Entrar al modo (K2 o `POST /mode`).

**Secuencia por parlante** (los `CAL_NUM_CH` = 32 de la DS11, ~8 s de scan):

1. `SETTLE` — 100 ms de silencio (decaimiento de la sala).
2. `ARM` → emite un **chirp de 10 ms** (500 Hz → 6 kHz lineal, −6 dBFS, fades
   de 32 muestras) por ese parlante solo, y arranca la captura del mic.
3. `CAPT` — captura 100 ms del mic.
4. `CORR` — correlación cruzada contra el chirp de referencia, **en chunks de
   128 lags por pasada del main loop** (~0.5 ms) para no perder deadlines de
   audio. Pico → tiempo de vuelo (interpolación parabólica → sub-muestra Q8);
   signo del pico → polaridad del parlante; calidad = pico/promedio ×10
   (≥ ~50 = medición sólida; menos imprime `LOW — mic signal?`).
5. Siguiente parlante… al terminar: `delay_k = max(tof) − tof_k` →
   `tx_delay_q8[]` + `speaker_sign[]`.

**`DONE`**: reproduce un tono de 1 kHz steered con los delays y polaridades
**medidos** — audiblemente enfocado en la posición del mic. `ADCpk c0` del
heartbeat = nivel en el foco. Para re-escanear, salir y volver a entrar.

- Las latencias fijas del pipeline (buffers, group delay de DAC/ADC) son
  idénticas para todos los parlantes → se cancelan en los delays relativos.
- Salir del modo aborta cualquier scan en curso.
- **No se restaura al boot** (ver Persistencia arriba).
- `cal_state` (0=idle … 5=done) se reporta en la telemetría `CMD_STATUS`.

## Modos 12–13 — TX tone (validación de direccionalidad sin mic)

Generador de tono **interno** (−6 dBFS) que se escribe en `beam_line` en las
mismas posiciones/cadencia que el RX beam, así el path de TX steering corre
**sin cambios** (delays fraccionales Q8, gains, signos). Plan de diseño:
`docs/ds10_tone_beam_plan.md`.

- Acumulador de fase Q16 sobre la tabla de seno de 48 muestras con
  interpolación lineal → el delay sigue siendo un delay de TIEMPO real a
  cualquier frecuencia (a diferencia del tono de `CAL_DONE`, cuyo truco de
  fase solo vale a 1 kHz).
- **Dither de frecuencia (Fase B)**: el tono se modula ±5 % en triángulo a
  ~3 Hz alrededor de la base (2 kHz → 1900↔2100). El haz es de retardos puros
  (banda ancha) → no cambia; los modos de la sala se promedian y la medición
  se estabiliza aunque la sala sea mala. Sin clicks (fase continua).
- `beam_out_shift = 0` en estos modos (el ×32 de los modos ADC clipearía
  brutalmente contra los PAM: el tono ya es −6 dBFS).
- Foco (0, **0**, 1000) mm: con las 4 filas pobladas en DS11 el foco va al
  centro del array y la apertura vertical (480 mm) también enfoca. En DS10
  iba a fy = −240 para apuntar al plano de la única fila poblada.
- **Test auto-validante**: caminar con un mic o el oído a 1 m. El spot a 2 kHz
  (~±14 cm) debe ser aproximadamente la mitad de ancho que a 1 kHz (~±27 cm).
  Relación ~2 = el steering funciona.
- Límite físico: mantener tono ≤ 2140 Hz (d = λ/2 con pitch de 80 mm) para
  evitar grating lobes.
- Nota de implementación: el tono se genera en `Process_ADC_Half()`, es decir
  su cadencia la marca el DMA RX del ADC — si SAI3_B RX se frena, estos modos
  quedan en silencio.

## Modo 14 — MULTIFOCUS 4 tonos (mapa de focos multiplexado en frecuencia)

> ✅ **VALIDADO EN HARDWARE (2026-07-12)**: caminando de izquierda a derecha los
> tonos aparecen de grave a agudo, coincidiendo con la geometría configurada.

**Precuela de la intervención de 8 entradas analógicas**: N fuentes, cada una
con su **propia tabla de delays focales** (`mf_delay_q8[src][ch]`) — la misma
infraestructura que después va a situar 8 señales reales (poesías, canales de
una banda, etc.) en 8 puntos distintos de la sala. Hoy las fuentes son tonos;
mañana serán `delay_line[src]` (los ADC), con las mismas tablas.

- **4 fuentes default**: 1300 / 1600 / 1900 / 2100 Hz en fx = −450 / −150 /
  +150 / +450 mm (fy = −240, fz = 1000). Frecuencias múltiplo de 100 Hz
  (fase exacta entre los 4 bancos de DACs), sin relaciones armónicas entre
  sí, todas bajo el límite de grating (2140 Hz).
- **Cómo se usa**: caminar la sala con un analizador espectral (Spectroid o
  similar): en cada posición domina el pico de la frecuencia cuyo foco está
  ahí. El mapa espacial se "lee" por frecuencia, sin mover nada.
- **Mover un foco individual** (sin re-entrar al modo):
  `POST /beam {"type":"mf","src":2,"fx":300,"fy":0,"fz":1000}` —
  re-apunta SOLO la fuente 2, conservando su frecuencia. `src` fuera de rango
  se rechaza sin tocar el modo en curso. Si no estabas en modo 14, un `mf`
  te mete en él.
- **Niveles**: cada tono sale a −6dBFS/4, así la suma coherente nunca pasa de
  −6dBFS (sin clipping aunque los 4 focos coincidan).
- Re-entrar al modo (K2 o POST) **resetea a los defaults**.
- Para senos, el delay se implementa como offset de fase exacto precalculado
  (matemáticamente idéntico al delay temporal, sin aproximación) — el foco es
  tan real como el de los modos 12/13.
- `gains[ch]`, mutes y polaridades medidas (`speaker_sign`) se aplican igual
  que en los modos TX.

## Modo 15 — MULTIFOCUS 8 entradas (la intervención)

**El modo para el que existe el instrumento**: cada una de las 8 entradas
analógicas del AK5538 se proyecta enfocada en su propio punto de la sala —
8 señales reales (poesías, canales de una banda, lo que sea) situadas en 8
lugares distintos, simultáneamente.

- **Fuente**: `delay_line[src]` (las entradas, capturadas siempre). Delay
  temporal verdadero con interpolación Q8 — vale para señal arbitraria, no
  solo senos. Mismas tablas de foco (`mf_delay_q8`) que el modo 14: el 14 es
  la versión "de laboratorio" de este modo, con tonos en vez de entradas.
- **Focos default**: entrada N → fx = −700 + 200·N mm (−700…+700, paso 200),
  fy = −240, fz = 1000. Re-entrar al modo resetea a estos defaults.
- **Mover una entrada de lugar** (en vivo):
  `POST /beam {"type":"mf","src":5,"fx":200,"fy":0,"fz":800}` — en este
  modo `src` va de 0 a 7 y re-apunta esa entrada sin tocar las demás.
- **Niveles**: boost de entrada +12 dB (`mf_in_shift = 2`, nivel de línea).
  Si distorsiona, **bajar el volumen de la fuente** (hay clamp, pero suena
  mal). Entradas sin nada conectado solo aportan piso de ruido, inaudible.
- Con 1–2 fuentes conectadas ya funciona (las entradas libres no molestan) —
  no hace falta esperar a tener las 8 señales.
- `gains[ch]`, mutes y `speaker_sign` se aplican igual que en los modos TX.

---

## Controles globales (afectan a todos los modos)

| Control | Vía | Efecto |
|---------|-----|--------|
| **Volumen maestro** | `POST /volume {"db":-20}` → `CMD_SET_VOLUME` | **Único maestro**: atenuación del PCM1690 (0…−63 dB, ≤−64 = mute), 0.5 dB/paso. Se persiste en EEPROM (debounce 3 s). Default −24 dB. |
| **Config de canales** | `POST /command` → `CMD_UPDATE_CFG` (52 bytes) | mute_global, gains/delays/mutes de los primeros 16 canales. `master_gain` ya **no se aplica** (volumen unificado en el DAC; el campo queda por compatibilidad). Se persiste (debounce 3 s). En modos RX beam, delays[0..7] pisan los delays de steering. |
| **Beam custom** | `POST /beam` → `CMD_SET_BEAM` | Steering fuera de los presets: `type:"tx"` con foco (fx,fy,fz) libre — **preserva el tono interno** si está activo (protocolo mic-fijo/foco-móvil); `type:"rx"` con az libre — **apaga el tono** y restaura el boost ×32; `type:"mf"` con `src` 0-7 — re-apunta el foco de UNA fuente MULTIFOCUS (tonos del 14 o entradas del 15, según el modo activo). No toca `ui_state_idx` — el próximo K2 retoma el ciclo desde el último preset. |
| **Telemetría** | `GET /status` → `CMD_GET_STATUS`/`CMD_STATUS` | modo actual, num_modes, az, foco, underruns, errores SAI, OVF del ADC, cal_state, picos ADC por canal, volumen maestro (`vol_db`, proto v2). |

## Diagnóstico por UART7 (115200 8N1, J7 → ST-Link VCP)

- Heartbeat 1/s: `[Ns] Half: 50/s, Full: 50/s, Err, Undr, RX bytes | ESP: <ip>`
- **`SYNC bancos: <n> us | CPU fills: <n>%`** — las dos métricas de DS11:
  - **SYNC** = desfase entre los ciclos de DMA de los dos masters SAI
    (SAI1_A = filas 0-1, SAI2_A = filas 2-3). **Debe dar pocos µs.** Si da
    milisegundos, los dos medios arrays están reproduciendo ventanas de
    tiempo distintas y el enfoque vertical no es coherente — ver la sección
    "Sincronismo entre bancos" abajo.
  - **CPU** = fracción del presupuesto (480M ciclos/s) gastada en los 4 fills.
    El valor que importa es en modo 15, que es el fill más caro.
- `ADCpk: c0..c7 | OVF RXdma=100/s` — picos de entrada + liveness del DMA RX.
- `BEAM az=<az> pk=` — solo en modos beam.
- Cada 10 s (verbose): estadísticas de fill (µs) **de SAI1_A solamente**,
  jitter y temperatura del CPU. Para el costo total usar la línea CPU.
- Reporte de test completo cada 60 s con PASS/FAIL del pipeline.

## Sincronismo entre bancos (crítico desde DS11)

`SAI1_A` (ch0-15, filas 0-1) y `SAI2_A` (ch16-31, filas 2-3) son **dos masters
asíncronos entre sí**: comparten PLL3 —misma frecuencia, sin deriva— pero cada
uno arranca su propio stream cuando se le pone `SAIEN`, y **ese offset inicial
queda fijo para siempre**. No hay nada en el hardware que los realinee: cada
grupo de DACs recibe su BCK/FS por pines distintos (PE4/PE5 → U2+U3,
PD12/PD13 → U1+U4), así que tampoco se pueden encadenar por SYNCEXT sin
cambiar la placa.

**Por qué importa**: el pipeline llena los 4 buffers tomando el mismo
`delay_write_pos` en una sola pasada del main loop. Eso sólo vale si los 4
callbacks caen en la misma pasada. Con los masters desfasados, los fills de
cada banco ocurren en pasadas distintas y pueden tomar valores de `wp` que
difieren en 480 muestras → **hasta ~10 ms (3,4 m de camino acústico) de
offset entre las filas 0-1 y las filas 2-3**.

**Regla firme**: los cuatro `HAL_SAI_Transmit_DMA` del arranque van
**consecutivos, sin nada en el medio** — en particular sin `UART_Print`, que
es bloqueante (~3,6 ms para 41 caracteres a 115200). Los prints van después.
Verificar siempre con la línea `SYNC` del heartbeat.

En DS10 esto era invisible porque sólo estaba poblado un banco (U2).
