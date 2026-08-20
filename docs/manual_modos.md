# Manual de modos de operación — DS11 (firmware STM32)

> Fuente: `firmware/Core/Src/main.c`, tabla `ui_states[]` (17 modos, índices 0–16).
> Última actualización: 2026-08-16.
>
> **¿Primera vez con el equipo?** Empezá por [guia_operacion.md](guia_operacion.md),
> que explica el conexionado y el encendido en lenguaje llano. Este manual asume
> que la placa ya está andando.

## ⚠️ Antes que nada: los amplificadores arrancan APAGADOS

Desde el incidente de brown-out del 2026-08-04, el pin SD de los 32 PAM8302A
**no** se habilita solo al final del boot. Ningún modo suena hasta que se los
enciende:

```bash
curl -X POST http://<IP>/amps -d '{"on":true}'     # encender
curl -X POST http://<IP>/amps -d '{"on":false}'    # apagar (botón de pánico)
```

> 💡 **`on` sirve además como comando de recuperación.** Desde 2026-08-17 siempre
> pulsa SD a 0 y de vuelta a 1, aunque ya estuviera encendido. Es por el **UVLO
> enclavado del PAM8302A**: tras cualquier hundimiento del +5VA, los amps que hayan
> cruzado 2,1 V quedan mudos hasta que se cicle SD o la alimentación — y como SD ya
> estaba alto, un "on" sin pulso no los recuperaba. Si alguna vez faltan canales sin
> causa visible, mandá `amps on` antes de sospechar del hardware.

Además de evitar el tirón de corriente, es la herramienta que separa limpio dos
hipótesis: si algo reinicia con amps ON y no con OFF, **es alimentación**; si
reinicia igual con OFF, hay que buscar en otro lado. Con los amps apagados corre
todo el pipeline —SAI, DMA, beamforming, los 4 bancos— en silencio absoluto.

## Cómo se cambia de modo

| Vía | Mecanismo |
|-----|-----------|
| **Botón K2** (PA4, presionar = LOW) | **Corto** (al soltar): avanza un modo, con wrap 15→0. **Largo** (≥1 s, actúa sin soltar): toggle de los amps. Debounce de 30 ms. |

> El K2 largo es lo que hace al instrumento **operable sin red**: los amps arrancan
> apagados y hasta ahora solo se encendían por HTTP, así que sin WiFi la placa
> quedaba muda sin forma de recuperarla desde el hardware. Como "on" republica el
> pulso de SD, también es el gesto de recuperación del UVLO enclavado de los
> PAM8302A. El modo se cambia **al soltar**, no al presionar: si no, toda pulsación
> larga cambiaría de modo antes de llegar a tocar los amps.
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
| 0 | `ADC monitor` | ADC ch0–7 en vivo | Las 8 entradas repetidas en cada fila | Escala por la ganancia de entrada, modo por defecto |
| 1 | `Channel walker (1s/ch)` | Tono 1 kHz interno | Un canal por vez, 1 s cada uno | **32 canales** = 32 s por vuelta |
| 2 | `RX beam az=0 (broadside)` | Beam delay-and-sum de los 8 ADC | Beam mono en todos los canales | Escala por la ganancia de entrada |
| 3 | `RX beam az=+30` | ídem, steering +30° | ídem | |
| 4 | `RX beam az=-30` | ídem, −30° | ídem | |
| 5 | `RX beam az=+60` | ídem, +60° | ídem | |
| 6 | `RX beam az=-60` | ídem, −60° | ídem | |
| 7 | `TX 2k ondulante LEJOS 2m` | Tono interno 2 kHz | 32 canales con delays focales | Foco (0, 0, 2000) mm — el par de profundidad con el 10 |
| 8 | `TX 2k ondulante izq 0.5m @1m` | ídem | ídem | Foco (−500, 0, 1000) mm |
| 9 | `TX 2k ondulante der 0.5m @1m` | ídem | ídem | Foco (+500, 0, 1000) mm |
| 10 | `TX 2k ondulante CERCA 0.3m` | ídem | ídem | Foco (0, 0, 300) mm |
| 11 | `AUTOFOCUS scan (mic->ADC c0)` | Chirp por parlante + mic | Scan → tono enfocado medido | Requiere mic en ADC c0 |
| 12 | `TX 2k ondulante centro @1m` | Tono interno 2 kHz | TX steering completo | Valida direccionalidad sin mic |
| 13 | `TX 1k ondulante centro @1m (spot 2x)` | Tono interno 1 kHz | ídem | Spot ~2× más ancho que el 12 |
| 14 | `MULTIFOCUS 4 tonos @1m` | 4 tonos internos (1.3/1.6/1.9/2.1 kHz) | Cada tono enfocado en su propio punto | Precuela de la intervención 8-in |
| 15 | `MULTIFOCUS 8 entradas @1m` | Las 8 entradas analógicas | Cada entrada enfocada en su propio punto | **La intervención**: 8 señales, 8 lugares |
| 16 | `TX 2k BARRIDO izq<->der @1m` | Tono interno 2 kHz | Foco barriendo ±700 mm, ciclo de 5 s | **Demuestra el steering sin caminar** |
| 17 | `BARRIDO VERTICAL 800Hz arriba<->abajo @1m` | Tono interno **800 Hz** | Foco barriendo ±600 mm en Y, ciclo de 5 s | **Es un experimento**, no una demo — decide un gasto de hardware en DS12 |

**IMPORTANTE (append-only)**: los índices están espejados en el ESP32, la web y
la app iOS. Modos nuevos se AGREGAN al final; nunca reordenar.

## Modo 16 — BARRIDO: la demostración que no hay que explicar

El foco recorre solo la sala, **±700 mm en un ciclo de 5 segundos**, con el mismo
tono ondulante de 2 kHz. Los presets 8/9 prueban el steering, pero exigen pararse
en un punto descentrado y **comparar dos modos de memoria**. Acá no hace falta
nada de eso: parado donde sea, el nivel **sube cuando el foco pasa por vos y baja
cuando se va**.

- Se reapunta 20 veces por segundo desde el main loop (32 raíces cuadradas por
  paso, ~3 µs). El paso de foco por actualización son ~14 mm = **0,04 muestras de
  delay**, así que no hay salto audible al reapuntar.
- Usa `Beam_SetTxFocus()`, la versión **silenciosa**: la que imprime la tabla de
  delays por fila sería inviable 20 veces por segundo.

> ⚠️ **El barrido del modo 16 es horizontal a propósito.** En vertical el array
> no enfoca a 2 kHz — las filas están a 160 mm = 0,93 λ, por encima del límite
> λ/2, así que produce lóbulos de rejilla en vez de un foco. Para probar el eje
> vertical hay que bajar la frecuencia: eso es el modo 17.

---

## Modo 17 — BARRIDO VERTICAL: un experimento, no una demo

**Este modo existe para responder una pregunta de hardware, y la frecuencia ES
la pregunta.** No lo uses para mostrarle el equipo a alguien; usá el 16.

El 2026-08-19 Fernando reportó que el eje vertical del modo 15 no se percibe.
La causa es geométrica y **no se arregla por firmware**:

| eje | apertura | paso | a 2 kHz (λ=172 mm) | techo sin lóbulos |
|-----|----------|------|--------------------|-------------------|
| horizontal (8 columnas) | 600 mm | 80 mm | 0,47 λ ✅ | **2144 Hz** |
| vertical (4 filas) | 480 mm | **160 mm** | **0,93 λ** ❌ | **1072 Hz** |

A 2 kHz el array vertical está **aliaseado**: en vez de un foco produce lóbulos
de rejilla. A 800 Hz (con el dither de ±5 %, 840 Hz máximo) el paso vertical es
0,37 λ — **dentro del límite en todo el rango de steering**.

### Cómo se lee el resultado

Parate quieto a ~1 m y escuchá el ciclo de 5 s. Hay **dos respuestas posibles y
las dos deciden plata**:

- **Se percibe movimiento vertical** → la causa era el aliasing. Bajar el paso
  de filas a ≤86 mm en la DS12 **se justifica**.
- **No se percibe** → el limitante son las 4 filas, no el paso. La apertura
  vertical de 480 mm son 1,12 λ a 800 Hz: haz de ~51°, spot de ±45 cm a 1 m.
  Achicar el paso **no alcanza** — hay que agregar filas.

Compará contra el modo 16 (mismo barrido, eje horizontal, 2 kHz), que es donde
el array sí tiene resolución: esa es la referencia de "así se siente cuando
funciona".

> La amplitud vertical es ±600 mm y no ±700 como en el 16, porque el haz
> vertical es mucho más ancho: barrer más sacaría el foco fuera de la zona donde
> el array todavía puede formarlo, y el recorrido se percibiría como un simple
> subir y bajar de volumen.

Detalle de la decisión de producto: `hardware/DS/DS12_PLANNING.md` §0.5.

---

## Las DOS ganancias, y por qué saturaba

**Hay dos controles de nivel y hacen cosas distintas.** Confundirlos fue el origen
del único problema serio de nivel que tuvo la DS11.

| | comando | qué es | dónde actúa |
|---|---|---|---|
| **Ganancia de entrada** | `POST /ingain {"db":N}` | cuánto se amplifica lo que ENTRA | **antes** del clamp digital |
| **Volumen maestro** | `POST /volume {"db":-N}` | atenuador del PCM1690 | **después**, a la salida |

### El síntoma y el diagnóstico

Con una Mac conectada, por encima del **~30 %** de volumen de la fuente el audio se
rompía. Sonaba a parlante exigido, pero no era eso: los PAM8302A son de 8 W y no se
los estaba exigiendo ni de cerca.

La medición **sticky** (pico sostenido entre impresiones, no una muestra al azar del
1 % del tiempo) resolvió la ambigüedad de una:

```
>>> SATURACION: ADC(OVF)=0  SALIDA(clip)=3152  <<<
```

**El ADC no clipeaba nunca. Clipeaba la salida digital, 3152 veces por segundo.**

> ⚠️ Ojo con medir el nivel muestreando: la línea `ADCpk` del heartbeat mira una
> ventana de 5 ms una vez por segundo — el 0,5 % del tiempo. Con eso llegué a
> escribir acá que "al ADC le sobraban 45 dB de headroom", y era falso: el pico real
> era −25 dBFS, no −45. Cualquier afirmación sobre niveles necesita pico sostenido.

### La causa: la cadena estaba al revés

```
ADC −25 dBFS ──[ +30 dB ]──► +4,6 dBFS ──[ CLAMP ]──► recorta ──[ −40 dB ]──► parlantes
                  ▲                          ▲                       ▲
            boost para mic             el techo real          atenuador del DAC
```

Se amplificaba **+30 dB para tirar −40 dB después**, con el recorte justo en el
medio. Por eso rompía a un nivel de fuente tan bajo: el techo no estaba donde
parecía. El +30 dB existía para poder escuchar un micrófono; con línea es
exactamente lo contrario de lo que hace falta.

### Cómo queda

La ganancia de entrada es **un parámetro global y persistente** (EEPROM, igual que el
volumen), no un default por modo: depende de **qué está enchufado**, no del modo de
beamforming activo. Escala toda la cadena alimentada por el ADC — monitor, RX beam y
MULTIFOCUS de entradas — con la única excepción de los modos de tono, cuya fuente
interna ya está a −6 dBFS.

- **Default de fábrica: 0 dB (línea).** Las entradas de la DS11 son de línea: J9 no
  lleva bias de micrófono, así que un electret necesita preamp externo igual.
- **+30 dB** es el preset "micrófono", para cuando lo que entra es de verdad débil.
- Se configura **una vez** y sobrevive al reboot.

```bash
curl -X POST http://$IP/ingain -d '{"db":0}'      # línea (default)
curl -X POST http://$IP/volume -d '{"db":-15}'    # subí acá, no allá
```

Con `/ingain 0` la fuente puede ir **al 100 %** sin recortar. Verificado en hardware
el 2026-08-18.

> 💡 **Cómo verificar el nivel de verdad**: la línea `SATURACION` del heartbeat solo
> aparece cuando hay algo que reportar, y distingue las dos causas. `ADC(OVF)>0` =
> bajá la fuente. `SALIDA(clip)>0` = bajá `/ingain` y compensá con `/volume`.

### Observación pendiente: diafonía entre canales del mismo cable

En esa misma sesión, con señal en c6/c7 (~20000-46000), los canales **c4 y c5 del
mismo cable RJ45** marcaban 150-330 contra 50-60 de los canales del *otro* cable.
Son unos **−38 dB** respecto de la señal.

Dos mecanismos posibles, no verificados: los **pares partidos** del pinout de J9
(ver `../hardware/DS/DS12_PLANNING.md` §6d), o —más probable con fuentes
desbalanceadas— la **impedancia común del retorno de masa**, que con una ficha
stereo comparte el cuerpo entre los dos canales. Vale la pena separarlo antes de
atribuirlo al cable.

## Modo 0 — ADC monitor

Ruta directa ADC→DAC para *escuchar* lo que captura el AK5538. Cada canal de
salida reproduce su canal ADC homólogo (DAC ch0 ← ADC ch0, etc.). En DS11 las
8 entradas se **repiten en cada fila** de 8 parlantes: la entrada N suena en la
columna N de las 4 filas. Se escala por la **ganancia de entrada** global (ver
arriba) y se clampea a ±FS 24-bit.

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
- Salida: el beam mono en **todos** los canales de salida, escalado por la
  **ganancia de entrada** global (`beam_out_q8 = in_gain_q8`) — misma cadena de
  ganancia que el monitor, así `/ingain` gobierna estos modos también.
- Azimut: 0 = broadside; positivo hacia índices de mic más altos.
- El heartbeat agrega la línea `BEAM az=<az> pk=<pico>`: con la misma señal
  inyectada en todos los mics, steering "hacia la fuente" ≈ ADCpk de un canal;
  steering lejos → cae. Esa diferencia es la validación del delay-and-sum.
- **Override experimental**: si llega `CMD_UPDATE_CFG` estando en modo beam,
  `delays[0..7]` del config se aplican como delays RX crudos por mic
  (pisan la tabla calculada hasta el próximo cambio de modo).

## Modos 7–10 — TX focus (steering de transmisión, presets)

Beamforming **de transmisión** near-field: un **tono interno de 2 kHz** que cada
canal de salida reproduce retrasado para que los frentes de onda converjan en el
punto focal. Caminando la sala se oye el spot moverse — el modo se valida solo,
sin instrumental y sin nada conectado a la entrada.

> **Cambio 2026-08-18: la fuente era el RX beam (los micrófonos).** El diseño
> original asumía micrófonos en la placa y la DS11 no los tiene: J9 son entradas
> de **línea sin bias**, así que estos modos reproducían lo que hubiera enchufado
> en el ADC y no se distinguían del modo 0 salvo por el retardo — que fue
> exactamente el reporte ("los modos TX suenan como RX"). Escuchar la entrada ya
> lo hacen los modos 2-6, y mucho mejor el 15. Lo que faltaba era **demostrar el
> steering**, y para eso la fuente tiene que ser interna. El modo 7 queda
> equivalente al 12; los que agregan valor son el 8, 9 y 10.

> **2 kHz, no 1 kHz** (corregido 2026-08-19 tras escucharlos). La apertura en X
> son 600 mm (8 columnas × 80 mm + 40 de ladrillo). A **1 kHz** eso es **1,75 λ**
> y el spot a 1 m sale de **±286 mm**: los focos de izquierda y derecha, separados
> 1 m, se solapan tanto que **desde el centro suenan idénticos** — que fue
> exactamente el reporte. A **2 kHz** la apertura es **3,50 λ** y el spot cae a
> **±143 mm**, el doble de resolución. El techo es **2144 Hz** (c/2d, lóbulos de
> rejilla), así que 2 kHz es lo mejor que puede hacer este array.
>
> ⚠️ **Estos modos NO se distinguen por el timbre** — es el mismo tono en los
> cuatro. Lo que cambia es **dónde** en la sala sube el nivel. Escucharlos parado
> en un solo punto no prueba nada.

### Por qué se llaman "ondulante" y no "tono"

No producen un tono puro: la frecuencia hace un **triángulo de ±5 % a 3 Hz**
(a 2 kHz, entre 1900 y 2100 Hz). Se llamaban "tono 2k", y eso confundía —
reportado el 2026-08-19 escuchándolos.

**No es un defecto ni algo cosmético.** Un tono puro en una sala arma **ondas
estacionarias** con nodos fijos en el espacio: caminando se escucharía el patrón
de la **sala**, no el foco del beam. El dither los emborrona, y por eso lo que se
oye es el beam de verdad.

Concretamente: la verificación del steering —modo 8 más fuerte parado 50 cm a la
izquierda, modo 9 más fuerte a la derecha, **sin espejar**— funcionó *gracias* a
la ondulación. Un tono puro habría dado un resultado mucho menos confiable.

> ✅ **Steering verificado a oído el 2026-08-19.** El 8 (foco −500 mm) suena más
> fuerte a la izquierda y más bajo a la derecha; el 9 al revés. **La orientación
> NO está espejada** — el marco del oyente mirando los conos es correcto y
> `spk_x_mm[]` no necesita cambio de signo. Dato firme para la DS12.
>
> El 10 (0,3 m) contra el 7 (1 m) también se percibe, aunque **es ambiguo**:
> acercarse al array sube el nivel por distancia, más allá del foco. Para que ese
> test sea concluyente conviene que el 7 enfoque más lejos (ver "Próximos pasos").

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
- Ganancia unitaria: el tono ya está a −6 dBFS (la ganancia de entrada NO se
  aplica acá — la fuente es interna y de nivel conocido).
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

### Se corre desde la página, sin cable serie (2026-08-20)

Hasta ahora el scan sólo se podía seguir por UART, y **terminaba diciendo
`done` aunque no hubiera medido nada**. Con el micrófono desenchufado o los
amplificadores apagados, la correlación elegía el máximo del *ruido*, el equipo
se quedaba con 32 retardos arbitrarios, pisaba un foco anterior que sí servía, y
después "reproducía un tono enfocado en el micrófono" que no estaba enfocado en
ningún lado. **El fallo era indistinguible del éxito.**

En `http://domosonica.local` hay una tarjeta **AUTOFOCUS** con cinco filas que
se pintan solas — rojo por defecto, porque "todavía no se comprobó" y "está mal"
se atienden igual:

| fila | verde cuando | de dónde sale |
|------|--------------|---------------|
| amplificadores | están encendidos | `state_flags` bit 0 |
| micrófono en la entrada 1 | c0 tiene señal, no sólo ruido | `mic_live_mask` bit 0 |
| nivel del micrófono | entre el umbral y −6 dBFS | `adc_pk[0]`, en dBFS |
| scan | terminó (ámbar mientras corre, con `N/32`) | `cal_state` + `cal_progress` |
| resultado | ≥16/32 parlantes medidos con confianza | `cal_ok_count` |

**Y ahora un scan malo NO se aplica.** Con menos de la mitad de los parlantes
por encima de `CAL_Q_MIN`, los retardos anteriores quedan **intactos** y el
serie dice qué revisar.

### El umbral de calidad era exactamente el valor del ruido

Primera versión de esta guarda: sin micrófono daba **4/32** una vez y **7/32**
otra. Fernando preguntó lo obvio —*¿no es raro que encuentre micrófonos si no
hay ninguno?*— y tenía razón: no era casual, era la **mediana de la distribución
del ruido**.

`cal_quality` es `peak/avg × 10` de la correlación. Correlacionando ruido blanco
contra el chirp, `|c|` es media-normal:

```
promedio  = σ·√(2/π)                        = 0,798 σ
máximo sobre N = CAL_MAX_LAG = 4320 lags
          ≈ σ·√(2·ln N) = σ·√(2·8,371)      = 4,09 σ
peak/avg  ≈ 5,13                            →  q10 ≈ 51
```

El umbral estaba en **50**. Cada canal era una moneda al aire y 4-7 de 32 caían
del lado bueno. **Un cociente siempre encuentra un máximo**: en cualquier
ventana de ruido hay un lag que sobresale.

Dos correcciones:

1. **`CAL_Q_MIN` 50 → 100.** Exige un máximo de ~8σ, cuya probabilidad sobre
   4320 lags es del orden de 5·10⁻¹²: nunca. Una detección real tiene ganancia
   de proceso de √480 ≈ 22 en amplitud, así que queda muy por encima.
2. **Gate absoluto sobre el pico de la captura** (`cal_cap_peak`), porque un
   cociente **no distingue señal de ruido fuerte**: si subís el preamp, el ruido
   crece arriba y abajo y el cociente ni se entera. Si en los 100 ms de captura
   el micrófono no pasó de `MIC_LIVE_THRESH`, la calidad se anula sin mirar la
   correlación.

**Verificado 2026-08-20**: tres scans seguidos sin micrófono → **0/32, 0/32,
0/32**. Determinístico, no lotería. El log por canal ahora imprime también
`cap=` (el pico de la captura) y distingue `(SIN SENAL en el mic)` de
`(BAJA — ruido de sala?)`, que son dos problemas distintos.

El botón *Iniciar scan* entra al modo 11; volver a entrar **relanza** el scan,
así que sirve igual para arrancar y para repetir después de corregir una fila.

> **Detección de micrófono**: pico de 1 s sobre *todas* las muestras ≥ 2000
> cuentas (escala 24 bits), con 3 s de histéresis para que una pausa al hablar
> no cuente como desconexión. El piso de ruido con las entradas al aire, medido
> el 2026-08-20, es de 437 cuentas máximo → el umbral queda 13 dB arriba. Un
> micrófono con preamplificador captando una sala en silencio lo cruza de sobra
> (−40 dBFS son 83886 cuentas).
>
> ⚠️ Los números de piso de ruido "43–224" que aparecen en comentarios viejos
> son de cuando el pico se calculaba sobre una ventana de 10 ms: **subestiman el
> piso real por un factor de ~2.**

## Modos 12–13 — TX tone (validación de direccionalidad sin mic)

Generador de tono **interno** (−6 dBFS) que se escribe en `beam_line` en las
mismas posiciones/cadencia que el RX beam, así el path de TX steering corre
**sin cambios** (delays fraccionales Q8, gains, signos). Plan de diseño:
los modos 12-13 de `manual_modos.md`.

- Acumulador de fase Q16 sobre la tabla de seno de 48 muestras con
  interpolación lineal → el delay sigue siendo un delay de TIEMPO real a
  cualquier frecuencia (a diferencia del tono de `CAL_DONE`, cuyo truco de
  fase solo vale a 1 kHz).
- **Dither de frecuencia (Fase B)**: el tono se modula ±5 % en triángulo a
  ~3 Hz alrededor de la base (2 kHz → 1900↔2100). El haz es de retardos puros
  (banda ancha) → no cambia; los modos de la sala se promedian y la medición
  se estabiliza aunque la sala sea mala. Sin clicks (fase continua).
- `beam_out_q8 = 256` (unidad) en estos modos: la ganancia de entrada NO se
  aplica acá — el tono ya es −6 dBFS y un ×32 clipearía brutalmente contra los
  PAM. La fuente es interna y de nivel conocido, no necesita ajuste.
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
- **El pad de la página web** es la forma normal de manejar este modo: vista en
  planta (X horizontal, Z = distancia, el array abajo) con 8 puntos arrastrables,
  uno por entrada, más un slider de altura para el punto seleccionado. Manda el
  mismo `POST /beam type:"mf"` de abajo, con throttle de 150 ms mientras se
  arrastra — cada POST espera el ACK del STM32 por UART a 115200, así que sin
  throttle la UI se traba encolando tramas.

  > ⚠️ **El "arrastre fantasma" (encontrado y corregido 2026-08-20).** Síntoma:
  > el equipo estaba en modo 0 y *"después de un rato empezaba a tirar un tono
  > solo"*, con `MFOCUS src0: ...` en el serie y `CPU fills` saltando a 61%. No
  > era el STM32: era la página mandando `POST /beam type=mf`, y **ese comando
  > apaga el monitor y enciende MULTIFOCUS**.
  >
  > Causa: si un `pointerup` se perdía —soltar fuera de la ventana, cambiar de
  > pestaña a mitad de arrastre, un gesto del trackpad— la variable `drag`
  > quedaba en ≥0 **para siempre**, y desde ahí **cualquier paso del mouse por
  > encima del pad re-apuntaba una fuente y cambiaba el modo, sin hacer clic**.
  > Se volvió frecuente justo cuando el pad pasó a ocupar todo el ancho: el
  > cursor lo cruza mucho más seguido, sobre todo al scrollear.
  >
  > Arreglo: en `pointermove`, `e.buttons == 0` significa que no hay ningún
  > botón apretado → el arrastre terminó. Más `pointerup` a nivel ventana,
  > `lostpointercapture` y `blur` como redes de seguridad.
  >
  > **Lección**: un comando que cambia el modo de operación no puede colgar de
  > una variable de estado de UI que sólo se limpia por el camino feliz.
- **Mover una entrada de lugar** (en vivo, equivalente por comando):
  `POST /beam {"type":"mf","src":5,"fx":200,"fy":0,"fz":800}` — en este
  modo `src` va de 0 a 7 y re-apunta esa entrada sin tocar las demás.
- **Niveles**: usa la **ganancia de entrada** global (`/ingain`, default 0 dB =
  línea). Si distorsiona, bajala y compensá con `/volume` — hay clamp, pero
  suena mal. Ojo que acá se suman 8 fuentes: si las 8 tienen señal fuerte, el
  acumulado puede recortar aunque cada una sola no lo hiciera. Entradas sin nada
  conectado solo aportan piso de ruido, inaudible.
- Con 1–2 fuentes conectadas ya funciona (las entradas libres no molestan) —
  no hace falta esperar a tener las 8 señales.
- `gains[ch]`, mutes y `speaker_sign` se aplican igual que en los modos TX.

---

## Controles globales (afectan a todos los modos)

| Control | Vía | Efecto |
|---------|-----|--------|
| **Amplificadores** | `POST /amps {"on":true}` → `CMD_SET_AMPS` (0x0A) | Enable del banco de 32 PAM8302A (pin SD, PD3). **Arranca en 0** — sin esto no suena nada. No se persiste: cada boot arranca apagado. |
| **Volumen maestro** | `POST /volume {"db":-20}` → `CMD_SET_VOLUME` | **Único maestro**: atenuación del PCM1690 (0…−63 dB, ≤−64 = mute), 0.5 dB/paso. Se persiste en EEPROM (debounce 3 s). Default −24 dB. |
| **Ganancia de entrada** | `POST /ingain {"db":0}` → `CMD_SET_INGAIN` | Ganancia de la etapa de ENTRADA, 0…36 dB en pasos de 1 dB (multiplicador Q8). Global, **no** por modo. Se persiste en EEPROM (debounce 3 s). Default 0 dB = línea; 30 dB = preset micrófono. Ver "Las DOS ganancias" arriba. |
| **Config de canales** | `POST /command` → `CMD_UPDATE_CFG` (102 bytes) | mute_global, gains/delays/mutes de **los 32 canales** (hasta 2026-08-18 eran 16 — media matriz: las filas 3 y 4 no se podían silenciar ni atenuar desde la red). ⚠️ El ESP32 manda siempre los 32: **los canales que no estén en el JSON van en cero**, o sea mudos. La respuesta HTTP ahora refleja el ACK del STM32, no sólo que se envió. `master_gain` ya **no se aplica** (volumen unificado en el DAC; el campo queda por compatibilidad). Se persiste (debounce 3 s). En modos RX beam, delays[0..7] pisan los delays de steering. |
| **Beam custom** | `POST /beam` → `CMD_SET_BEAM` | Steering fuera de los presets: `type:"tx"` con foco (fx,fy,fz) libre — **preserva el tono interno** si está activo (protocolo mic-fijo/foco-móvil); `type:"rx"` con az libre — **apaga el tono** y restaura el boost ×32; `type:"mf"` con `src` 0-7 — re-apunta el foco de UNA fuente MULTIFOCUS (tonos del 14 o entradas del 15, según el modo activo). No toca `ui_state_idx` — el próximo K2 retoma el ciclo desde el último preset. |
| **Telemetría** | `GET /status` → `CMD_GET_STATUS`/`CMD_STATUS` | modo actual, num_modes, az, foco, underruns, errores SAI, OVF del ADC, cal_state, picos ADC por canal, volumen maestro (`vol_db`, proto v2), ganancia de entrada y los dos contadores de saturación (`ingain_db`, `adc_ovf`, `out_clip`, proto v3), estado real de amps y beam custom (`amps`, `custom`, proto v4), uptime del STM32 (`stm32_uptime_ms`, v5) y **% de CPU del pipeline** (`cpu_fills_pct`, v6). |

> ⚠️ **`mode` puede mentir, y `custom` es el que avisa.** `POST /beam` cambia lo
> que suena **sin** tocar `ui_state_idx` — a propósito, para que el próximo K2
> retome el ciclo desde el último preset elegido. La consecuencia es que la página
> puede mostrar "modo 13" mientras la placa hace otra cosa; costó una sesión de
> debug el 2026-08-18. Con `custom: true` la UI lo dice. Lo limpia elegir un modo.
>
> `amps` también es estado real, leído del firmware: antes la web sólo recordaba
> el último botón apretado, y eso se vuelve falso con un reboot o con el K2 largo.

> **`underruns` y `sai_errors` también son acumulativos desde proto v5.** Antes
> el resumen de 60 s los ponía en cero, así que el número de `/status` parecía
> acumulativo y retrocedía solo cada minuto — medir por diferencia daba
> resultados imposibles. Y `uptime_ms` era el del **ESP32**: un reinicio del
> STM32 no se veía desde la red. Ahora `/status` trae `stm32_uptime_ms` aparte.
>
> **Los contadores de saturación son MONOTÓNICOS**: lo que importa es que *crezcan*
> entre dos lecturas — eso es "está saturando ahora", no "saturó alguna vez". Antes
> se ponían en cero al imprimirse en el heartbeat, y así no se podían exponer por
> telemetría (el que leyera primero se los comía). Ahora el heartbeat imprime el
> delta contra su propia lectura anterior y `/status` publica el acumulado.
>
> ⚠️ `ds_status_t` ocupa **63 de los 64 bytes** de `PROTO_MAX_PAYLOAD`. El próximo
> campo obliga a subir ese límite **en ambos lados** — si no, el ESP32 descarta la
> trama por larga y el status se queda mudo sin dar error.

## Diagnóstico por UART7 (115200 8N1, J7 → ST-Link VCP)

- Heartbeat 1/s: `[Ns] Half: 50/s, Full: 50/s, Err, Undr, RX bytes | ESP: <ip>`
- **`SYNC bancos: <n> us | CPU fills: <n>%`** — las dos métricas de DS11:
  - **SYNC** = desfase entre los ciclos de DMA de los dos masters SAI
    (SAI1_A = filas 0-1, SAI2_A = filas 2-3). **Debe dar pocos µs.** Si da
    milisegundos, los dos medios arrays están reproduciendo ventanas de
    tiempo distintas y el enfoque vertical no es coherente — ver la sección
    "Sincronismo entre bancos" abajo.
  - **CPU** = fracción del presupuesto (480M ciclos/s) gastada en los 4 fills.
    **Valor de referencia medido (2026-08-14, soak de 19,6 h): 26,6% en modo 7**,
    con `Fill avg = 668 µs`. Si volvés a ver ~60%, revirtió el I-cache o el `-O2`
    (ver `ds11_perf.md`). El valor en modo 15, el fill más caro, sigue sin anotarse.
- `ADCpk: c0..c7 | OVF RXdma=100/s` — picos de entrada + liveness del DMA RX.
- `BEAM az=<az> pk=` — solo en modos beam.
- Cada 10 s (verbose): estadísticas de fill (µs) **de SAI1_A solamente**,
  jitter y temperatura del CPU. Para el costo total usar la línea CPU.
- Reporte de test completo cada 60 s con PASS/FAIL del pipeline.

**Sobre `Undr` (underruns)**: hasta el 2026-08-14 subía ~2/s de forma constante y
eso era **audible** — un "pop" por segundo, y uno más fuerte cada 10 s. La causa
eran los prints bloqueantes y el guardado de EEPROM, que superaban el deadline de
10 ms del fill. Se corrigió haciendo que `UART_Print` y el ciclo de escritura de la
EEPROM **sirvan los fills mientras esperan** (`Service_Fills()` / `EEPROM_Yield()`).
**Ahora el valor esperado es 0 sostenido.** Si vuelve a subir, algo bloquea el main
loop más de 10 ms y se va a escuchar.

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
**consecutivos, sin nada en el medio** — en particular sin `UART_Print`. Los
prints van después. Verificar siempre con la línea `SYNC` del heartbeat.

**✅ Medido (2026-08-14): 4 µs al principio, 1 µs (839 ciclos) tras las
optimizaciones, y estable arranque tras arranque a lo largo de 19,6 h.** Son 1,4 mm
de camino acústico contra un pitch de 80 mm entre parlantes: irrelevante. La regla
funcionó y el riesgo está cerrado. Como el offset es reproducible, una calibración
de AUTOFOCUS **sí sobreviviría** un ciclo de alimentación.

En DS10 esto era invisible porque sólo estaba poblado un banco (U2).
