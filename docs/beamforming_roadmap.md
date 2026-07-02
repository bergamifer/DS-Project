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

### Fase 2 — TX steering desde geometría

`Fill_Audio_Beamform` pasa a leer de UNA delay line del beam (no de `delay_line[mic]`): `out[k] = sign[k] × beam_dline[i − tx_delay[k]] × gain[k]`. La infraestructura `delays[32]`/`gains[32]`/`mutes` de `beam_config` ya existe. Delay TX por punto focal near-field: `d_k = |p − x_k|/c`, `delay_k = round((max_j d_j − d_k)·fs)`, con ventana (Hann) para sidelobes. Cálculo en el STM32 (M7 con FPU, geometría en una sola fuente de verdad); el ESP32 manda solo `{azimut, elevación, distancia_focal, nivel}`. Revisar `MAX_DELAY_SAMPLES=64` (45cm de apertura) contra la geometría real del array.

### Fase 3 — Precisión sub-muestra (solo si la medición lo pide)

A 48kHz, 1 muestra = 7.1mm. Para voz (<4kHz) el error de cuantización es ±7° de fase — empezar con delays enteros y medir el patrón antes de complicar. Escalera de upgrade: interpolación lineal (+1 MAC/muestra/canal) → FIR polifásico windowed-sinc 8 taps × 32 fases (8 MAC/muestra/canal, ~3-5% CPU para TX completo 32ch). Dual beam (L+R) duplica un costo que ya es marginal. Calibración por canal (offsets ±2 samples, trim de gain) persistida en EEPROM — el lazo ADC→DAC que ya funciona permite auto-calibración.

### Fase 4 — Protocolo / ESP32 / web

`uart_protocol.h` hoy: `CFG_NUM_CHANNELS=16`, mutes uint16. Extender con comandos versionados: `CMD_SET_BEAM {mode, az, el, dist_mm, nivel}` (uso normal), `CMD_RAW_CFG_32` (delays/gains/mutes de 32ch, para experimentos), `CMD_CAL` (offsets por canal). ESP32: endpoint `POST /beam` con UI de sliders; definir trama binaria de status (underruns, OVF, peaks) para que `/status` tenga telemetría real.

---

## Próximo hito medible mínimo

**"Beam RX que sigue a la fuente"** = Fase 0 (fix de wp ✅ + channel walker) + Fase 1 (RX delay-and-sum). Criterio de éxito sin hardware nuevo: con una fuente sonora fija, el peak meter del heartbeat sobre `beam[]` muestra máximo cuando el steering apunta a la fuente y cae ≥6dB al apuntar lejos (ganancia de array teórica con 8 mics: 9dB sobre un mic solo). No depende de los DACs despoblados ni del arnés de parlantes — valida toda la cadena geometría→delays→suma que después se reusa idéntica del lado TX.

---

*Roadmap derivado de la auditoría de firmware asistida (Fable, 2026-07-02) — ver también [ds11_upgrades.md](ds11_upgrades.md) para los hallazgos de hardware de la misma revisión.*
