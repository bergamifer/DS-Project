# DS10 — Mediciones de mañana + backlog de implementación

> Escrito 2026-07-10. Contexto: primer encendido con 8 parlantes (fila 1 / U2). El walker
> validó el mapeo (ch0-7 = izq→der). Al reproducir tono se oyeron nodos de sala pero no se
> encontró el foco concreto → casi seguro los **modos propios de la sala** tapan el foco.

---

## ✅ RESULTADOS 2026-07-12 — beamforming TX validado en el aire

Primera sesión de campo con el firmware nuevo (dither + volumen unificado + modo 14):

1. **Foco TX audible de oído**: boost sutil y detectable en `{"type":"tx","fx":1000,
   "fy":-240,"fz":4000}` (4 m, 1 m a la derecha). "Sutil" es lo ESPERADO a esa
   distancia: N Fresnel ≈ 0.13 → régimen de haz dirigido (~14° de azimut), no de foco
   puntual; lóbulo ~1.1 m de ancho. El steering funciona.
2. **MULTIFOCUS validado (modo 14)**: 4 tonos simultáneos (1300/1600/1900/2100 Hz),
   cada uno enfocado en su punto (fx −450/−150/+150/+450 @ 1m). **Caminando de
   izquierda a derecha, los tonos aparecen de grave a agudo — el mapa espacial
   coincide con la geometría configurada.** En una sola prueba quedó validado:
   - la superposición de N tablas de delay focales independientes,
   - el mapeo canal→parlante (consistente con el walker),
   - la convención de signos X del marco del oyente,
   - la lectura espacial por frecuencia (analizador espectral como "GPS" del campo).

   El modo 14 es la **precuela funcional de la intervención de 8 entradas**: las
   mismas tablas `mf_delay_q8[8][32]` van a situar 8 señales reales (poesías,
   canales de una banda, …) en 8 puntos de la sala — solo cambia la fuente
   (`delay_line[src]` en vez de tonos).

Pendiente de esta serie: cuantificar el contraste (teléfono con FFT o ECM8000+2i2,
guía en `docs/autofocus_setup.md`) y el test 2k-vs-1k de ancho de spot (~2×).

---

## A. Protocolo de medición para mañana (sin firmware nuevo)

### Principio clave: mic FIJO, mover el FOCO
No mover el micrófono por el campo de estacionarias (el nivel salta por la sala, no por el
haz). Dejar el mic quieto y steerear el foco **encima y afuera** de él: el modo de sala en
ese punto es constante y se cancela; lo que cambia es puro haz.

### Condiciones
- **Espacio abierto o absorbente** (el instinto es correcto — la sala chica y dura fue el
  problema). Afuera, o con frazadas/absorbente alrededor.
- **Distancia 0.5-1 m, NO 2 m.** La física va al revés: a 2 m el foco es más ancho pero la
  mitad de fuerte (nº de Fresnel N: 0.5m≈0.91, 1m≈0.46, 2m≈0.23). Más cerca = campo directo
  domina las reflexiones = más contraste.
- **Frecuencia: 2 kHz (modo 12).** Es la que mejor enfoca (foco ±15cm @1m vs ±27cm a 1kHz)
  sin lóbulos de rejilla (límite d=λ/2 en 2.14kHz).
- **Volumen** conservador (`/volume -20` o más), para no saturar el mic ni la sala.

### Medición con 1 mic (o teléfono con app de SPL)
Mic fijo en el objetivo, sobre el eje del centro del array, a z≈1 m.
1. Foco sobre el mic:  `POST /beam {"type":"tx","fx":0,"fy":-240,"fz":1000}` → anotar `ADCpk c0`.
2. Foco +40cm al lado: `POST /beam {"type":"tx","fx":400,"fy":-240,"fz":1000}` → anotar `c0`.
3. Foco −40cm:         `fx:-400` → anotar.
4. La caída de (1) respecto a (2)/(3) = contraste focal, con el modo de sala **cancelado**
   (mismo punto físico). Esperar varios dB de diferencia si el foco existe.
5. Repetir a z=500 y z=1500 para ver cómo cambia el contraste con la distancia.

### Medición con 2 mics ECM8000 (lo mejor — requiere preamp con phantom)
- Mic A → entrada 1 (c0), Mic B → entrada 2 (c1). El heartbeat ya imprime ambos: **no hace
  falta firmware nuevo.**
- Mic A en el objetivo, Mic B como referencia 40cm al costado.
- Steerear foco sobre A (`fx:0`): sube c0, c1 queda. Steerear sobre B (`fx:+400` si B está a
  la derecha): sube c1, c0 queda. Ese cruce es la prueba más convincente e inmune a la sala.

### Prueba cualitativa (por oído, sin instrumento)
Barrer el foco lentamente de lado a lado (`fx` de −400 a +400) parado en el punto objetivo:
si el instrumento enfoca, se oye pasar el nivel. (Automatizable — ver backlog #4.)

### Qué NO medir todavía
- Modos RX beam (2-6): usan geometría de mics placeholder de 8 canales que **nunca va a
  existir** (máx 2 mics). El instrumento es un **proyector TX**; los mics son solo medición.

---

## B. Backlog de implementación (para cuando vuelva el crédito)

Ordenado por valor:

1. ✅ **HECHO 2026-07-11** — **Dither de frecuencia (Fase B del plan de tono)** — el matador
   de modos de sala. Triángulo ±5% a 3Hz sobre `tone_base_hz`, actualizado por half-buffer
   (pasos ≤0.3%/10ms, fase continua → sin clicks). Activo siempre en modos 12/13.

2. ✅ **HECHO 2026-07-11** — **Unificar el volumen** — el `(s*master)>>8` salió del fill TX;
   `/volume` (atenuador DAC) es el único maestro. `gains[ch]` y `mute_global` conservados.
   El campo `master_gain` queda en el struct solo por compatibilidad de layout EEPROM.

3. **Modo de medición con 2 mics + contraste automático** — un modo que steerea el foco
   ON/OFF del mic automáticamente y reporta por UART el Δ dB (contraste focal) usando c0 (y
   c1 como referencia diferencial). Convierte el protocolo manual de arriba en un número.

4. **Modo "haz audible" (Fase C del plan de tono)** — `fx` barre −600→+600mm en ~6s. Parado
   enfrente se oye pasar el haz. Validación cualitativa instantánea sin instrumental.

5. **Resiliencia WiFi de la app** — tolerar 2-3 polls fallidos antes de marcar offline
   (los cortes por cambio de canal del router se vuelven invisibles). Swift puro.

6. **mDNS en el ESP32** (`domosonica.local`) — dejar de cazar la IP.

7. **Ventaneo de Hann por canal** — usa `gains[ch]` para bajar lóbulos laterales. Recién
   cuando se pueda medir el patrón (con el dither #1 y el contraste #3 andando).

8. **CMD_GET_CAL_RESULT** — exponer delays/signos/calidad del AUTOFOCUS por protocolo.
   Requiere mic + preamp; el AUTOFOCUS de 1 mic ya existe.

---

## C. Bugs de una review cruzada (2026-07-10) — verificados contra el código

Priorizados. El #1 tiene un matiz importante para el protocolo de medición (§A).

1. ✅ **FIXEADO 2026-07-11 (por rama, como se especifica abajo).**
   **`UI_ApplyCustomBeam` no maneja el estado del tono** (main.c ~2230). Apaga
   walker/monitor/autofocus pero NO toca `tone_enabled` ni `beam_out_shift`.
   - Efecto: en modo tono (12/13), un `POST /beam type=rx` deja el tono sonando → oís
     el tono de 2kHz creyendo que escuchás los mics.
   - **FIX POR RAMA (no blanket):** apagar el tono SOLO en la rama `type=rx`
     (`tone_enabled=0; beam_out_shift=5;`), y **preservarlo en `type=tx`** (steerear el
     foco del tono es un caso válido — es el protocolo de medición §A). Un fix que apague
     el tono en ambas ramas rompería la medición.
   - **Para las mediciones de §A `type=tx` YA funciona bien** con el código actual; el bug
     solo afecta `type=rx`, que no se usa mañana.

2. ✅ **FIXEADO 2026-07-11** (debounce unificado `CFG_SAVE_DEBOUNCE_MS` para modo, volumen
   y `CMD_UPDATE_CFG`; el slider ya se puede arrastrar).
   **El volumen guarda en EEPROM sin debounce** (main.c ~964). Cada `CMD_SET_VOLUME`
   dispara un save inmediato (~12ms bloqueo ≈ 6 underruns). Arrastrar el slider de volumen
   de la web = ráfaga de glitches. Reusar el `mode_save_due_tick` (debounce 3s) para el
   volumen y para `CMD_UPDATE_CFG`. **Para mañana: cambiá el volumen con `curl` a valores
   discretos, no arrastres el slider web.**

3. **ACK/NACK bloqueante en el ISR** (uart_protocol.c ~262). `send_ack` bloquea ~0.4ms
   dentro del handler de UART4, y si el main loop está en `UART_Protocol_SendStatus`
   (mismo UART) el ACK devuelve BUSY y se pierde → el ESP32 hace timeout. Encolar el ACK y
   mandarlo desde el main loop.

4. **Carrera en `current_config`** (uart_protocol.c ~216 vs main.c ~1001). El ISR hace
   `memcpy` mientras el main loop la lee; dos configs muy seguidas → lectura mezclada.
   Copiar bajo `__disable_irq()` o doble buffer.

5. **UART7 mezcla TX bloqueante + IT** — `UART_Protocol_PrintConfig` manda ~1200 bytes por
   IT (~104ms) mientras `UART_Print` es bloqueante sobre el mismo UART7 → durante esos
   ~100ms los prints devuelven BUSY y se pierden en silencio (el "Config applied" que sigue
   probablemente nunca sale). No rompe nada, explica logs faltantes.

6. **Walker: desfase de canal entre bancos en el cruce de segundo** (main.c ~2509).
   Cosmético con solo U2 poblado; en DS11 (32ch) puede dar un half de 10ms con dos
   parlantes o ninguno en cada transición de segundo.

7. **Código muerto**: `test_tone_enabled` no se setea nunca (main.c ~183) y el path
   "default" de `Fill_Audio_Beamform` (playback con `beam_config.delays[]` por canal) es
   inalcanzable con el set actual de modos. Borrarlos o marcarlos — hoy sugieren un
   comportamiento de `CMD_UPDATE_CFG` que no existe.

**Menores** — ✅ ambos fixeados 2026-07-11: `eeprom_write_page` ya solo espera los 6ms
cuando la escritura fue OK; el header de eeprom_config.c ahora dice **AF6** (el AF11 del
design review era un error, confirmado en el bring-up).

> Nota de honestidad: mi primera revisión (más arriba en la sesión) dijo "sin bugs" — se me
> pasó el #1 por no cubrir `UI_ApplyCustomBeam`. La review cruzada fue más exhaustiva.

---

### Nota de arquitectura confirmada (2026-07-10)
- El instrumento es un **proyector TX**. Los micrófonos (máx **2** ECM8000) son herramientas
  de **medición/calibración**, no beamforming RX. MIC_COUNT=8 y los modos RX beam quedan como
  código muerto — no invertir en ellos.
- ECM8000: condenser, **necesita +48V phantom + preamp** (la placa da nivel de línea, no
  phantom). Sin preamp, medir con app de SPL del teléfono (el protocolo mic-fijo/foco-móvil
  funciona igual).
