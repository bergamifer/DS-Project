# Roadmap hacia beamforming real

**Base:** auditoría de arquitectura del firmware (asistida por IA, modelo Fable, 2026-07-02) sobre `firmware/Core/Src/main.c`, más el estado de hardware de [ds10_bringup_findings.md](ds10_bringup_findings.md) y [ds11_upgrades.md](ds11_upgrades.md).
**Estado actual:** `Fill_Audio_Beamform()` con delays=0/gains=max es un passthrough — reproduce un mic por salida, sin suma. El beamforming real (delay-and-sum multi-mic) todavía no existe.

---

## Auditoría de main.c — hallazgos

La arquitectura base es sana: ISRs mínimas que solo marcan flags, fill diferido en el main loop, doble buffer circular en RAM_D2, contadores de diagnóstico completos. Con ~578µs de fill por buffer de 10ms, sobra ~94% de CPU para el beamforming real.

### ✅ Resuelto (2026-07-02)

- **Carrera de `delay_write_pos`**: los 4 fills TX leían el puntero por separado y el RX se procesaba después de los 4 en el loop. Invisible en passthrough, pero rompería la coherencia de fase entre bancos de DAC con delays reales. Fix: RX se procesa primero en el loop, todos los TX comparten el mismo `wp` de la pasada. Ver [ds10_bringup_findings.md](ds10_bringup_findings.md#hallazgo-8).
- **Heartbeat bloqueante**: recortado el bloque verboso de cada 10º heartbeat (quedó solo Fill/Jitter/Temp), bajó el costo de ~15 a ~4 underruns por bloque.

### 🟡 Pendiente — bajo impacto, no bloqueante

- `delay_line` vive en `.dma_buffer`/RAM_D2 pero ningún DMA lo toca (solo CPU) — moverlo a DTCM (0-wait-state) aceleraría el hot-loop gratis. Optimización, no urgente mientras sobre presupuesto de CPU.
- `PE12` configurado en modo ANALOG (línea "unused pins" del init) — en la **DS11** es el driver de LEDs de los 4 jacks RJ45 (R111-R114 del netlist). No aplica a la DS10 actual (no tiene esos jacks); reconfigurar cuando llegue hardware DS11.
- Residual de underruns del heartbeat (~4/bloque) — bajar la frecuencia del bloque verboso de cada 10 a cada 30-60 heartbeats si se quiere que el test automático de 60s pase con `Underruns==0` (hoy `Err` y jitter, que son las métricas reales, están perfectas).
- `test_tone_phase` avanza una vez por bloque pero hay 4 llamadas/ciclo — benigno hoy por casualidad aritmética (480 % 48 == 0), frágil si cambia el tamaño de buffer o la frecuencia del tono.
- Cosmética desactualizada: banner sigue diciendo "DS10" en comentarios de SAI2_B que ya no aplican a la DS11 (SD en PE11, no PD8).

### Tablas específicas de DS11 (mapeo/polaridad) — diferidas

Los hallazgos de swap JSPK09/10 y polaridad alternada por canal (ver [ds11_upgrades.md](ds11_upgrades.md)) son del netlist **DS11**, una placa que todavía no está fabricada. No tiene sentido codificar `speaker_map[32]`/`speaker_sign[32]` en el firmware actual (que corre sobre DS10) con valores no verificables en el hardware que hay en banco. Se implementan cuando llegue la DS11 física, validados con el channel walker (Fase 0 abajo).

---

## Fases

### Fase 0 — Fundaciones (bloqueante para beamforming real)

1. ~~Fix de la carrera de `wp`~~ ✅ hecho 2026-07-02.
2. Tablas de geometría en firmware: `speaker_map[32]` (canal DSP → DAC/slot), `speaker_sign[32]` (polaridad), `speaker_pos[32]` y `mic_pos[8]` en mm — **requiere DS11 física** (ver arriba).
3. **Channel walker**: tono secuencial de 1s por canal + print del canal activo. Con un multímetro en modo AC o un mic en el cono, verifica mapeo Y polaridad física en una pasada. Se puede correr hoy mismo en la DS10 actual para los 8 canales de U2, y sirve como plantilla para validar la DS11 cuando llegue.

### Fase 1 — RX delay-and-sum (el beamforming real mínimo)

Nuevo buffer `beam[480]`: por muestra, `beam[i] = Σₘ (mic_gain[m] × delay_line[m][(wp − rx_delay[m] − 480 + i) & MASK]) >> 3`. Son 8 MAC/muestra = 3840 MACs/10ms — despreciable (~0.1% CPU). `rx_delay[m]` se calcula desde `mic_pos` y la dirección deseada: far-field `τ_m = (x_m·u(θ)) / c`, normalizado a que el mínimo sea 0, cuantizado a muestras. Con delays=0 ya es un beam broadside válido.

### Fase 2 — TX steering desde geometría ✅ implementada 2026-07-02 (pendiente validación acústica)

`Fill_Audio_Beamform` en modo TX lee del beam RX (`beam_line`) con delay focal por canal: `out[k] = sign[k] × beam_line[i − tx_delay[k]] × gain[k] × master`, respetando mutes. Geometría real cargada: **array 4×8 patrón ladrillo, X_pitch=80mm, Y_pitch=160mm, filas 2&4 corridas +40mm** (marco del oyente de frente a los conos; fila 1 = ch0-7 = U2). `Beam_InitTxGeometry()` construye `spk_x/y_mm[32]` (ch = fila×8+col, placeholder a validar con channel walker en DS11); `Beam_UpdateTxFocus(fx,fy,fz)` calcula el delay near-field por canal (`d_k = |p−spk_k|/c`, el más lejano dispara primero) relativo al centro del array (300,240)mm, imprime la tabla 4×8 por serial. `TX_MAX_DELAY_SAMPLES=128` — la diagonal del array (768mm ≈ 108 muestras) acota por desigualdad triangular cualquier foco posible; la delay line de 1024 admite hasta 544. `tx_delay[32]` es deliberadamente **separado** de `beam_config.delays[]` (que el ESP32 sobreescribe en cada comando y que en modo beam dobla como delays RX crudos); gains/mutes/master sí se comparten. 4 presets de foco appendeados al ciclo K2 (estados 8-11: centro@1m, izq0.5m@1m, der0.5m@1m, centro@0.3m) sin tocar los 7 existentes. Delays de referencia verificados contra cálculo Python independiente (centro@1m: domo simétrico max 9; laterales: rampas espejadas max 39; NEAR: domo max 25).

Pendiente de esta fase: ventana de ganancias (Hann) para sidelobes, comando ESP32 `{az, el, dist, nivel}` (hoy los presets son solo por K2), y la validación acústica con parlantes conectados. La ventana Hann conviene recién cuando se pueda medir el patrón — con 1 solo DAC poblado no hay apertura completa.

### Fase 3 — Precisión sub-muestra ✅ interpolación lineal implementada 2026-07-02 (branch `feature/beamforming`)

`tx_delay_q8[32]` en punto fijo Q8 (1 LSB = 1/256 de muestra ≈ 0.03mm de camino acústico); `Beam_UpdateTxFocus` conserva la fracción en vez de redondear y el fill TX interpola linealmente entre las dos muestras adyacentes de `beam_line` (intermedio int64 — el Δ×frac desborda int32). Es el primer escalón de la escalera prevista; el FIR polifásico windowed-sinc queda para cuando una medición del patrón lo justifique (a 48kHz el error lineal-vs-sinc solo pesa arriba de ~4kHz). Los delays RX siguen enteros (la geometría de mics es placeholder — no tiene sentido refinar sub-muestra sobre posiciones inventadas). Dual beam: pendiente, sin caso de uso definido todavía. Calibración por canal: cubierta de forma más fuerte por el modo AUTOFOCUS (abajo); persistencia EEPROM de la calibración queda pendiente (el resultado del autofocus es específico de la posición del mic — decidir si vale persistirlo como "último foco" antes de tocar el layout de EEPROM).

### Modo AUTOFOCUS — foco medido con micrófono ✅ implementado 2026-07-02 (branch `feature/beamforming`)

**Idea (del usuario): elegir el punto focal físicamente con un micrófono.** Mic de medición colocado en el punto deseado → ADC input 1 (c0). El modo (estado 12 de K2) escanea: chirp lineal 500Hz→6kHz de 10ms por parlante (uno a la vez, resto en silencio, 100ms de settle entre canales), captura 100ms del mic, y correlaciona contra la referencia — **chunked a 128 lags por pasada del loop (~0.6ms)** para no romper jamás el deadline de audio de 10ms. Por parlante se obtiene: **TOF sub-muestra** (parábola sobre |corr| alrededor del pico, Q8), **polaridad medida** (pico de correlación negativo = parlante invertido → `speaker_sign[k]` — resuelve empíricamente el hallazgo de polaridad alternada del netlist DS11 sin depender del arnés), y **calidad** (pico/promedio ×10; <50 = señal de mic dudosa, se avisa). Con todos los TOF: `delay_k = max(tof) − tof_k` → `tx_delay_q8`. Los offsets fijos del pipeline (espera de buffer TX, group delays del DAC/ADC) son idénticos para todos los parlantes y **se cancelan en los delays relativos** — por eso no hace falta conocerlos. Al terminar, reproduce un tono 1kHz steered con los delays/signos medidos: **el foco queda físicamente en el mic**, y `ADCpk c0` del heartbeat mide el nivel en el foco (mover el mic y ver caer el nivel = validación del patrón). Escanea `CAL_NUM_CH=8` (solo U2 poblado); en DS11 se sube a 32. Re-entrar al modo re-escanea. Scan completo ~2s.

Esto es superior a la calibración "±2 samples de trim" planteada originalmente: no corrige un modelo — **mide el sistema real completo** (geometría, mapeo, polaridad, group delays) de una pasada.

### Control remoto de modos desde el webserver — PENDIENTE (spec para implementar)

Hoy el cambio de modo es solo por botón K2. Para hacerlo remoto (pedido 2026-07-02):

1. **Protocolo UART (ESP32→STM32)** — `uart_protocol.h` ya define framing `AA 55 | cmd | len(2,LE) | payload | checksum | 0D 0A` con `CMD_UPDATE_CFG=0x01`. Agregar:
   - `CMD_SET_MODE = 0x02`, payload 1 byte = índice de estado (0-11, mismo orden que `ui_states[]`). El STM32 responde ACK/NACK (ya existen `CMD_ACK`/`CMD_NACK`).
   - `CMD_GET_STATUS = 0x03`, payload 0. Respuesta nueva `CMD_STATUS = 0x04` con payload: `{uint8 mode_idx, uint8 num_modes, int16 beam_az, uint32 underruns, uint8 ovf, int32 adcpk[8]}` — le da al webserver telemetría real para UI.
   - En el STM32: el handler llama `UI_ApplyState(idx)` (ya es la única puerta de entrada de cambio de modo — K2 usa la misma función, así que no hay estado duplicado).
2. **ESP32** (`firmware_esp32c3/main/main.c`): endpoint `POST /mode` con JSON `{"mode": N}` → arma la trama `CMD_SET_MODE`; `GET /status` extendido para pedir `CMD_GET_STATUS` por UART y devolver el estado real del STM32 (hoy `/status` solo refleja estado local del ESP32). Los nombres de los modos pueden hardcodearse en el ESP32 (espejo de `ui_states[]`) o agregarse a la respuesta de status — versionar con un byte de versión de protocolo para que ESP32 y STM32 puedan flashearse por separado.
3. **STM32→ESP32 (estados/eventos)**: hoy el STM32 solo manda debug ASCII por UART4. Definir trama binaria de evento `CMD_EVENT = 0x05` (ej. "scan AUTOFOCUS terminado", "modo cambiado por K2") para que el webserver se entere de cambios locales sin polling.

### Fase 4 — Protocolo / ESP32 / web ✅ implementada 2026-07-02 (branch `feature/beamforming`)

Comandos nuevos sobre el framing existente: **CMD_SET_MODE (0x02)** — cambia entre los 12 estados remotamente, entra por `UI_ApplyState()` (la misma función que K2, sin estado duplicado); **CMD_SET_BEAM (0x07)** — steering custom fuera de presets (RX az arbitrario o foco TX x/y/z en mm) vía `UI_ApplyCustomBeam()`; **CMD_GET_STATUS (0x03) → CMD_STATUS (0x04)** — telemetría `ds_status_t` de 53 bytes versionada con `proto_ver` (modo, az RX, foco TX, underruns, errores SAI, OVF, estado del AUTOFOCUS, peaks de los 8 ADC). El parser STM32 ahora acepta payload len=0.

ESP32: parser de tramas binarias en RX (ACK/NACK + STATUS con checksum) conviviendo con el debug ASCII; request/response sincronizado por semáforos. Endpoints: `POST /mode {"mode":N}`, `POST /beam {"type":"rx","az":30}` / `{"type":"tx","fx":0,"fy":0,"fz":1000}`, `GET /status` (telemetría real). **`GET /` sirve una UI web autocontenida**: botones de los 12 modos con indicador del activo, sliders de az RX y foco TX, VU meter de las 8 entradas y panel de estado con refresh de 1s — control completo desde el teléfono.

Pendiente de esta fase: `CMD_RAW_CFG_32` (el `CMD_UPDATE_CFG` de 16 canales sigue vigente para eso), `CMD_EVENT` push STM32→ESP32 (hoy la web se entera por polling), y los nombres de modos duplicados en el ESP32 (espejo de `ui_states[]` — actualizar juntos, o mover los nombres a la respuesta de STATUS en una versión futura del protocolo).

---

## Próximo hito medible mínimo

**"Beam RX que sigue a la fuente"** = Fase 0 (fix de wp ✅ + channel walker) + Fase 1 (RX delay-and-sum). Criterio de éxito sin hardware nuevo: con una fuente sonora fija, el peak meter del heartbeat sobre `beam[]` muestra máximo cuando el steering apunta a la fuente y cae ≥6dB al apuntar lejos (ganancia de array teórica con 8 mics: 9dB sobre un mic solo). No depende de los DACs despoblados ni del arnés de parlantes — valida toda la cadena geometría→delays→suma que después se reusa idéntica del lado TX.

---

## Horizonte siguiente: multi-nodo (10-15 muros)

Una vez validada la Fase 1 en un nodo, el escalón siguiente es coordinar varios muros
DS11 sin cable de clock (distribución analógica autosincronizante) — arquitectura,
presupuesto de fase, y plan de fases A/B/C completo en
[multinode.md](multinode.md). El primer cambio de firmware que ese documento pide,
y que conviene hacer ni bien esta Fase 1 esté validada, es el **arranque
determinístico del pipeline** (TX arranca en el flanco del primer callback de RX,
§2 de ese doc) — deja el nodo listo para el experimento de 2 placas sin esperar a
tener la DS11 en mano.

---

*Roadmap derivado de la auditoría de firmware asistida (Fable, 2026-07-02) — ver también [ds11_upgrades.md](ds11_upgrades.md) para los hallazgos de hardware de la misma revisión.*
