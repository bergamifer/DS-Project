# Plan: generador de tono interno para validar direccionalidad (DS10, 8 parlantes)

> Escrito 2026-07-10. Para implementar en `feature/beamforming`.
> **Contexto**: la DS10 tiene solo U2/DAC0 poblado → 8 parlantes = **array lineal**
> (paso 80 mm, apertura 560 mm). Todo el steering TX ya está implementado pero
> **nunca se validó acústicamente**, porque `Fill_Audio_Beamform()` en modo TX lee de
> `beam_line`, que sale del **ADC**. Sin nada conectado a las entradas, los presets de
> foco están steereando ruido de fondo (-80 dBFS) → silencio.
>
> **Objetivo**: generar la señal internamente para poder medir el haz **sin micrófono
> en la placa** (el ECM8000 necesita phantom + preamp; queda para después).

---

## 1. Física: por qué 2 kHz

Array lineal, `d = 80 mm`, `L = 7×80 = 560 mm`, `c = 343 m/s`.

| f | λ | Ancho de haz -3dB (campo lejano) | Mancha focal @1m (-3dB, semi-ancho) | Primer nulo @1m | Lóbulos de rejilla |
|---|---|---|---|---|---|
| **1 kHz** | 343 mm | ~31° | ±27 cm | ±61 cm | no (`d/λ`=0.23) |
| **2 kHz** | 172 mm | ~16° | **±14 cm** | **±31 cm** | no (`d/λ`=0.47) |
| 2.14 kHz | 160 mm | — | — | — | **límite**: `d = λ/2` |

- **Usar 2 kHz como principal**: la mancha focal es **la mitad de ancha** que a 1 kHz →
  la direccionalidad es mucho más fácil de ver/medir.
- **No pasar de 2.14 kHz**: ahí `d = λ/2` y aparecen **lóbulos de rejilla** (haces
  falsos). La regla `d ≤ λ/2` garantiza estar limpio a *cualquier* ángulo de steering.
- **1 kHz sirve como contraste**: ver §5, es la prueba que se auto-valida.

### Campo cercano, no lejano
Distancia de Fraunhofer `2L²/λ`: **1.83 m** @1kHz, **3.66 m** @2kHz. A 1 m estás en
**campo cercano** → lo que medís es una **mancha focal**, no un patrón angular de campo
lejano. Está bien: es exactamente lo que el instrumento hace. Para un patrón de
directividad clásico habría que irse a ≥3.7 m (difícil en una sala).

---

## 2. Diseño: escribir la fuente interna en `beam_line`

**La clave**: NO tocar la ruta TX. Escribir la señal generada **en `beam_line`**, en las
mismas posiciones y cadencia en que la escribiría el beam del ADC. Así el steering TX
(delays fraccionarios Q8, gains, signos, mutes) funciona **sin ningún cambio**.

Ventajas frente a un camino paralelo analítico (como el tono de `CAL_DONE`):
- Sirve para tono, ruido, chirp o lo que sea — la fuente es agnóstica.
- El retardo sigue siendo un **retardo temporal real** (independiente de la frecuencia),
  no un offset de fase válido solo a 1 kHz. *(El tono de AUTOFOCUS asume `1 muestra = 1
  índice de tabla`, que solo vale a 1 kHz — no copiar ese patrón.)*
- Reusa la interpolación fraccionaria que ya está probada.

### Dónde
En `Process_ADC_Half()`, que ya corre a la cadencia del RX DMA (480 muestras) y define
el `wp` que las 4 llenadas TX comparten en la misma pasada:

```c
/* Los writes a delay_line[] (mics) quedan SIEMPRE — alimentan AUTOFOCUS y monitor. */

if (tone_enabled) {
    for (uint32_t i = 0; i < samples; i++)
        beam_line[(wp + i) & DELAY_LINE_MASK] = tone_next_sample();
} else if (beam_enabled) {
    /* ... RX delay-and-sum existente, sin cambios ... */
}
```

### Generador (acumulador de fase Q16 sobre la tabla de 48)
`sine_table_1khz[48]` es exactamente un período de 1 kHz @48 kHz. Paso por muestra en
índices de tabla: `48·f/48000 = f/1000`.

```c
static volatile uint8_t  tone_enabled  = 0;
static volatile uint16_t tone_freq_hz  = 2000;
static uint32_t tone_phase_q16 = 0;      // en indices de tabla, Q16

// inc = f * 65536 / 1000   → 1kHz: 65536 (paso 1.0) | 2kHz: 131072 (paso 2.0)
static inline int32_t tone_next_sample(void) {
    uint32_t inc = ((uint32_t)tone_freq_hz * 65536u) / 1000u;
    uint32_t idx = (tone_phase_q16 >> 16) % 48u;
    uint32_t nxt = (idx + 1u) % 48u;
    int32_t  fr  = (int32_t)(tone_phase_q16 & 0xFFFF);
    int32_t  s0  = sine_table_1khz[idx];
    int32_t  s1  = sine_table_1khz[nxt];
    int32_t  s   = s0 + (int32_t)(((int64_t)(s1 - s0) * fr) >> 16);
    tone_phase_q16 += inc;
    if (tone_phase_q16 >= (48u << 16)) tone_phase_q16 -= (48u << 16);  // wrap sin clicks
    return s;                                 // pico 2^22 = -6 dBFS
}
```

---

## 3. ⚠️ Detalles críticos (leer antes de tocar nada)

### 3.1 `beam_out_shift` DEBE ser 0 en modos de tono — o quemás amplificadores
La ruta TX termina con `s <<= beam_out_shift`, y `beam_out_shift = 5` (**+30 dB**), puesto
ahí porque el beam del ADC es debilísimo. Un tono a -6 dBFS × 32 = **clipping brutal**.

→ En `UI_ApplyState()`: `beam_out_shift = st->tone ? 0 : 5;`

**Presupuesto de nivel con `beam_out_shift = 0`:**
- Tono: **-6 dBFS** digital · atenuación PCM1690 **-24 dB** → **-30 dBFS** analógico.
- El PAM8302A clipea a ~**-22 dBFS** a la salida del DAC → **8 dB de margen**. Seguro.
- Si hace falta más SPL para medir: bajar la atenuación del PCM1690 hasta **-18 dB**
  como piso (deja -24 dBFS, 2 dB de margen). **Nunca por debajo de -16 dB.**

### 3.2 `fy = -240 mm` en los presets de tono
Los focos son **relativos al centro del array (300, 240) mm**, pero solo la **fila 1
(y=0)** está poblada. Como un array lineal **no discrimina Y**, con `fy=0` (relativo) el
rango efectivo es `√(240² + z²)`, no `z`. Poniendo **`fy = -240`** el foco cae en el
plano de la fila → la geometría del ensayo queda limpia y medís a la altura de los conos.

### 3.3 Espejo de modos en el ESP32
`ui_states[]` (STM32) y `MODE_NAMES[]` (`firmware_esp32c3/main/main.c`) son espejos
manuales. **Appendear al final** (no insertar) para no correr los índices existentes, y
actualizar **ambos** en el mismo commit.

---

## 4. Implementación por fases

### Fase A — núcleo (imprescindible)
1. Estado `tone_enabled`, `tone_freq_hz`, `tone_phase_q16` + `tone_next_sample()`.
2. Rama en `Process_ADC_Half()` que escribe `beam_line` (§2).
3. Campo `.tone` + `.tone_hz` en `ui_state_t`; `beam_out_shift = st->tone ? 0 : 5`.
4. Modos nuevos **appendeados** a `ui_states[]` (y a `MODE_NAMES[]` del ESP32):
   - `12: "TX tono 2k foco @1m"` → `beam=1, tx=1, tone=1, tone_hz=2000, fx=0, fy=-240, fz=1000`
   - `13: "TX tono 1k foco @1m"` → igual, `tone_hz=1000`
5. El steering se hace con lo que ya existe: **`CMD_SET_BEAM` type=tx** con `fx/fy/fz`.
   Para barrer ángulo a radio constante R: `fx = R·sin θ`, `fz = R·cos θ`, `fy = -240`.

### Fase B — medición confiable en una sala viva
Un **tono puro** en una sala genera ondas estacionarias: mover el mic 8 cm (λ/2 @2kHz)
puede cambiar el nivel varios dB **por reflexiones, no por el haz**. Esto puede enmascarar
por completo la mancha focal.

→ **Dither de frecuencia**: modular `tone_freq_hz` lentamente ±5 % (ej. 1900↔2100 Hz a
~3 Hz). Como delay-and-sum es de **retardos puros (banda ancha)**, el haz no cambia, pero
los modos de la sala se promedian y la medición se estabiliza. El acumulador de fase
mantiene continuidad → sin clicks.

*(Alternativas: medir cerca (≤1 m, donde domina el campo directo), amortiguar la sala con
frazadas, o medir afuera.)*

### Fase C — opcional
- `CMD_SET_TONE {uint16 freq_hz, uint8 enable, uint8 sweep_pct}` + slider en la app.
- Modo **"barrido audible"**: `fx` de -600 a +600 mm en ~6 s. Parado enfrente, **oís pasar
  el haz**. Demo instantánea, cero instrumental.

---

## 5. Cómo medir (sin micrófono en la placa)

**Instrumento**: el iPhone con una app de RTA/SPL (o la propia app Domosonica). No hace
falta conectar nada al ADC.

1. Teléfono a **z = 1 m**, sobre el eje, **a la altura de la fila** de parlantes.
2. Modo 12 (2 kHz). Anotar dB.
3. Mover lateralmente en pasos de 5-10 cm hasta ±60 cm. Anotar dB en cada punto.
4. Repetir en modo 13 (1 kHz).

### Predicciones (esto es lo que valida el firmware)

| | -3 dB | Primer nulo |
|---|---|---|
| 2 kHz | ±14 cm | ±31 cm |
| 1 kHz | ±27 cm | ±61 cm |

### Criterios de éxito
1. **La mancha existe**: hay un máximo claro en el foco y cae ≥6 dB antes de ±40 cm (2 kHz).
2. **La prueba que se auto-valida** 🎯: la mancha de **1 kHz debe ser ~2× más ancha** que
   la de 2 kHz. Esta razón **no depende de ninguna calibración absoluta** (ni de SPL, ni
   del mic, ni de la sala) — solo de que la cadena geometría → delays → interpolación Q8
   esté bien. Si la razón da ~2, el beamforming TX está **correcto**.
3. **Steering**: con `fx = +300 mm`, el pico se corre ~30 cm hacia ese lado.

---

## 6. Nota aparte: resiliencia WiFi de la app (tarea separada)

Del log del ESP32 (2026-07-09): el router hizo un **channel switch** (CSA, canal 10→11) y
**el ESP32 lo siguió sin desconectarse**. Lo único que murió fueron los **sockets TCP
abiertos** (`httpd_sock_err: error in recv : 104` = ECONNRESET, lo cierra el iPhone).

→ No hace falta acceso al router ni debuggear el iPhone. El arreglo es **en la app**:

- `AppState`: **no** pasar a `offline` ante **un** poll fallido. Exigir 2-3 fallos
  consecutivos antes de cambiar de estado (y reintentar). ~15 líneas.
- ESP32 (robustez): `config.lru_purge_enable = true` y subir `max_open_sockets` en
  `httpd_config_t` → cuando el pool se llena, purga el más viejo en vez de rechazar.
- Futuro: **mDNS** (`domosonica.local`) → la app no depende del IP.

Con eso, los cortes por cambio de canal se vuelven invisibles.
