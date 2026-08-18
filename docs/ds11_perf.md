# DS11 — Performance del pipeline de audio (2026-08-13)

> Sesión de medición sobre la placa DS11 reparada, con los 32 canales activos.
> Resultado corto: **el modo 15 no entraba en el presupuesto de CPU y ahora sí.**
> El costo de fill bajó ~43× en el peor modo con tres cambios, ninguno de los
> cuales toca el algoritmo de beamforming.

---

## 1. Punto de partida — los números que faltaban desde el bring-up

Del log de UART7 con la placa en **modo 7 (TX focus centro @1m)**:

```
[9s] Half: 50/s, Full: 50/s, Err: 0, Undr: 43, RX: 22 bytes | ESP: 192.168.100.64
     SYNC bancos: 4 us (2038 cyc) | CPU fills: 59.2%
     ADCpk: c0=52 c1=54 c2=58 c3=52 c4=52 c5=63 c6=58 c7=60 | OVF=0 RXdma=100/s
     Fill: min=716617 avg=716862 max=735039 cyc (1492/1493/1531 us)
     Jitter: period min=9599926 avg=9599973 max=9600012 cyc (range=86 = 0 us)
     Temp: 35.1 C
```

### ✅ `SYNC bancos: 4 us (2038 cyc)` — la duda de julio queda cerrada

El riesgo que motivó toda la instrumentación era que **SAI1_A y SAI2_A son masters
asíncronos**: si sus ciclos de DMA arrancaban desfasados, las filas 0-1 y 2-3 del
array estarían reproduciendo instantes distintos de la misma onda y el enfoque
vertical no sería coherente. Se temía que el desfase pudiera llegar a ~10 ms
(3,4 m de camino acústico).

**Medido: 4 µs.** Eso es 1,4 mm de camino acústico — **tres órdenes de magnitud por
debajo** del pitch de 80 mm entre parlantes, y 0,2 muestras a 48 kHz. Irrelevante.

El fix de arrancar los cuatro `HAL_SAI_Transmit_DMA` **consecutivos**, sin ningún
`UART_Print` en el medio, funcionó. Es estable arranque tras arranque (siempre
2038 ciclos, no varía). La nota de `ds11_bringup_checklist.md` sobre que una
calibración de AUTOFOCUS podría no sobrevivir un ciclo de alimentación **queda sin
efecto**: el sincronismo es reproducible.

### ✅ La tabla de delays es correcta — verificada contra la geometría

El log imprime el foco calculado por fila:

```
r1: 4.88 7.28 8.82 9.49 9.27 8.16 6.18 3.38
r2: 3.38 6.18 8.16 9.27 9.49 8.82 7.28 4.88
r3: 1.46 3.81 5.32 5.97 5.75 4.67 2.74 0.00
```

A primera vista parece un bug: para un foco **centrado**, cada fila debería ser
simétrica sobre sí misma, y r1/r2 (equidistantes del centro vertical) deberían ser
**idénticas**. En cambio r2 es el **espejo** de r1.

Es correcto, y lo confirma la geometría en `main.c`:

```c
#define SPK_ROW_DX_MM   40.0f    // brick offset of rows 2&4 (r=1,3)
spk_x_mm[ch] = (float)c * SPK_X_PITCH_MM + ((r & 1) ? SPK_ROW_DX_MM : 0.0f);
```

El array es **brick pattern**: las filas impares están corridas +40 mm (medio pitch)
en X. Con el centro del array en x=300 mm:

- fila par (r=0,2): x relativo = `[-300 -220 -140 -60 +20 +100 +180 +260]`
- fila impar (r=1,3): x relativo = `[-260 -180 -100 -20 +60 +140 +220 +300]`

que es exactamente `-reverse(fila par)`. Como r1 y r2 tienen el **mismo |y|** (±80 mm
del centro) y x espejado, sus delays salen espejados. **La asimetría del print es la
firma del brick offset**, o sea evidencia positiva de que el código de geometría está
haciendo lo que debe. r3 contiene el 0.00 porque es una fila exterior y su elemento
más alejado del foco es el que dispara primero.

### ⚠️ `CPU fills: 59.2%` y `Undr` subiendo — acá estaba el problema

`Err: 0` y jitter 0 µs, pero los underruns **crecían continuamente** a ~4-5/s, y el
test de 60 s terminaba siempre en `>>> TEST FAILED`.

---

## 2. Los underruns tenían dos causas distintas

### Causa A — el propio instrumento de medida (los prints bloqueantes)

`UART_Print` es `HAL_UART_Transmit` **bloqueante** a 115200 baudios. El heartbeat
imprime ~238 caracteres por segundo:

| línea | chars |
|---|---|
| `[Ns] Half: … ESP: …` | ~82 |
| `SYNC bancos … CPU fills …` | ~54 |
| `ADCpk: c0=… RXdma=…` | ~80 |
| `BEAM az=… pk=…` | ~22 |

238 chars × 86,8 µs/char = **20,7 ms bloqueado**, contra un deadline de fill de
**10 ms**. Durante ese bloqueo pasa un callback que deja el flag `need_fill_*`
puesto, y el siguiente lo encuentra sin limpiar → `underrun_count++` **por cada uno
de los 4 bancos** = **+4 por segundo**. Exactamente lo observado.

Y cada 10 s el bloque `verbose` agrega 3 líneas más (~210 chars = 18 ms) → un
deadline extra perdido → el salto de **+4 a +10** que se ve en el log:

```
[11s] … Undr: 51        [21s] … Undr: 97         [31s] … Undr: 143
[12s] … Undr: 61  (+10) [22s] … Undr: 107 (+10)  [32s] … Undr: 153 (+10)
```

**El `TEST FAILED` era el instrumento midiéndose a sí mismo.** Dicho esto, no es
inocuo: cada underrun significa que el DMA re-emite un medio buffer viejo, o sea un
salto de fase de 10 ms en los 32 canales a la vez, una vez por segundo.

### Causa B — el pipeline estaba corriendo a una fracción de su velocidad

`Fill: avg=716862 cyc` es el costo de llenar **un banco, un medio buffer** = 480
muestras × 8 canales = 3840 muestras de salida.

> **716862 / 3840 = 186 ciclos por muestra de salida.**

El lazo interno del modo 7 (`tx_steer_enabled`) hace: leer `tx_delay_q8[gch]`, dos
lecturas de `beam_line[]`, una interpolación lineal, un multiply por `gains[gch]`,
un shift, saturación, y un store. **~30 instrucciones.** 186 ciclos para 30
instrucciones es un factor ~6 de penalidad.

La causa es que **ninguna de las dos cachés del Cortex-M7 estaba habilitada**: no
había ni un `SCB_EnableICache()` ni un `SCB_EnableDCache()` en todo el proyecto. El
M7 corría a 480 MHz buscando **cada instrucción desde flash con 4 wait states**
(`FLASH_LATENCY_4`). El core estaba esencialmente al ralentí esperando la flash.

Y el `CPU fills: 59,2%` cierra la cuenta: 4 bancos × 2 mitades = 8 fills por ciclo de
buffer de 20 ms = 400 fills/s → 400 × 716862 / 480e6 = **59,7%**. ✅

---

## 3. Los tres cambios

### 3.1 Habilitar el I-cache

```c
/* USER CODE BEGIN 1 */
SCB_EnableICache();
/* USER CODE END 1 */
```

**Sin riesgo de coherencia**: el I-cache sólo cachea fetch de instrucciones desde
flash, y el código no se automodifica. El DMA no escribe en flash.

Se verificó además que **ningún timing depende de la velocidad de ejecución**:
`pcm1690.c` usa sólo `HAL_Delay` (basado en SysTick, independiente del I-cache), y
el bit-bang de MCLK de 8000 ciclos que existía en DS9 para el POR de los PCM1690 ya
no está en el firmware de DS11.

**El D-cache sigue apagado a propósito.** Habilitarlo obligaría a hacer mantenimiento
de caché en cada callback sobre los buffers DMA de RAM_D2, y el intento anterior
(`SCB_CleanDCache_by_Addr` sin D-cache habilitado) terminó en HardFault. Queda como
trabajo futuro, con MPU configurando RAM_D2 como no-cacheable.

### 3.2 Sacar `delay_line` de la memoria lenta

```c
-static int32_t delay_line[8][DELAY_LINE_SIZE] __attribute__((section(".dma_buffer"), aligned(32)));
+static int32_t delay_line[8][DELAY_LINE_SIZE] __attribute__((aligned(32)));
```

`.dma_buffer` está mapeado a **RAM_D2** (0x30000000), que se alcanza por el bus AXI.
Eso es obligatorio para los buffers que toca el DMA — pero **`delay_line` no es uno
de ellos**: el DMA del ADC escribe en `audio_buffer_3b_rx`, y es la **CPU** la que
copia de ahí a `delay_line` en `Process_ADC_Half`. Estaba en memoria lenta sin
necesitarlo.

Ahora vive en **DTCMRAM** (0x20006de0, 32 KB), acoplada al core y con **0 wait
states**. Es la estructura más leída del pipeline: el modo 15 hace 8 lecturas por
muestra de salida y por canal. DTCM queda en ~60 KB de 128 KB, con ~68 KB para stack.

### 3.3 `-Og` → `-O2`

```makefile
OPT = -O2
```

`-Og` es "optimizar para depurar". En un lazo DSP que corre 15360 veces por cada
10 ms, la diferencia con `-O2` es enorme. El binario incluso quedó **más chico**
(71452 → 69368 bytes de text).

---

## 4. Resultado medido

Método: contar el incremento de `underruns` vía `GET /status` durante 20 s en cada
modo (script en `scratchpad/undr.sh`, descarta los reset del test de 60 s). El máximo
teórico es 400/s = 8000 en 20 s (todos los deadlines perdidos en los 4 bancos).

| modo | `-Og`, sin caché | + I-cache + DTCM | + `-O2` | mejora |
|---|---|---|---|---|
| **7** — TX focus centro @1m | 100 | 48 | **14** | **7×** |
| **15** — MULTIFOCUS 8 entradas | 4407 | 2189 | **102** | **43×** |

Traducido a deadlines perdidos:

- **Modo 15 antes**: 4407/20 s = 220/s sobre 400/s = **se perdía el 55% de los
  fills**. El modo era inusable: el fill pedía más del 100% del tiempo disponible.
- **Modo 15 ahora**: 102/20 s = 5/s = **~1,3%**, y ese resto es casi todo el
  bloqueo de los prints (causa A), no falta de CPU.

> **El modo 15 —la intervención de 8 fuentes— pasó de no entrar en el
> presupuesto a entrar con margen.** Es el modo más caro del firmware (8 fuentes ×
> 32 salidas = 256 taps de delay line por muestra) y es el objetivo artístico del
> instrumento, así que era exactamente el que había que salvar.

---

## 5. Pendiente

### ✅ Prueba de escucha del `-O2` — APROBADA (2026-08-18)

> **`>>> TEST PASSED: DMA pipeline clean! <<<` con `Total Underruns: 0`** — primera
> vez en todo el proyecto. Modos 1, 12, 0 y 14 verificados a oído: **sin crujidos,
> sin ruido de fondo, sin pops.** Las 8 entradas analógicas capturando.
>
> El `-O2` queda validado. No hubo bugs latentes de `volatile`.

Y el arreglo del pop del volumen quedó **medido**, no sólo escuchado: 6 cambios de
volumen + un guardado de EEPROM + un pulso de amps, todo con `underruns = 0`. Antes,
cada cambio costaba ~9,6 ms de bloqueo, o sea al menos un deadline perdido.

<details><summary>La instrucción original (histórica)</summary>

#### Prueba de escucha del `-O2` — obligatoria antes de confiar

Cambiar el nivel de optimización puede destapar bugs latentes de `volatile` en
código compartido entre ISR y main loop. La telemetría dice que está bien
(`sai_errors: 0`, el pipeline corre, los modos responden), pero **eso no prueba que
el audio sea correcto**. Hay que escuchar:

1. Modo 1 (walker): los 32 canales, uno por uno, limpio.
2. Modo 12/14: tono sin distorsión ni artefactos.
3. Modo 15: con una fuente conectada, que la voz esté situada y sin glitches.

Si algo suena mal, revertir es una línea (`OPT = -Og` en `firmware/Makefile`) y el
problema queda acotado a un bug de `volatile` que vale la pena encontrar igual.

</details>

### ✅ Underruns residuales — RESUELTO (2026-08-14): eran audibles

Fernando escuchó **"un pedito cada vez que se imprime el serial"** en modo 12. Eso
cerró el caso: los underruns no eran un contador feo, eran **audio corrupto**. Cada
uno es medio buffer reemitido en los 32 canales a la vez = un salto de fase de
10 ms, que en un tono continuo se oye como un pop.

Este documento decía que la causa A "no es un problema del pipeline de audio". Era
cierto sobre la **causa** y falso sobre la **consecuencia**.

Había tres bloqueos, todos por encima del deadline de 10 ms:

| bloqueo | duración | underruns |
|---|---|---|
| heartbeat (~238 chars @115200) | ~20 ms | +2/s |
| bloque `Fill/Jitter/Temp` (cada 10 s) | ~18 ms extra | +8 |
| `EEPROM: Saving config` (2 páginas) | ~12 ms | +12 |

**El arreglo: usar el tiempo muerto en vez de acortarlo.**

1. El despacho de fills salió del main loop a **`Service_Fills()`** (con guard de
   reentrada, defensivo por si algún fill imprime en el futuro).
2. **`UART_Print` dejó de ser `HAL_UART_Transmit`**: ahora manda byte a byte y, en
   los ~87 µs que espera el TXE de cada carácter, llama a `Service_Fills()`. Que la
   UART quede idle unos µs de más entre bytes no corrompe nada — sólo espacia los
   caracteres, y la consola se ve igual.
3. La EEPROM usa un hook **`EEPROM_Yield()`** (`__weak` en `eeprom_config.c`,
   redefinido en `main.c`) en lugar del `HAL_Delay(6)` del ciclo de escritura. El
   chip tarda lo mismo; la diferencia es que el MCU ya no se queda mirando.

**Resultado medido: `underruns = 0` sostenido**, tanto en modo 12 como en modo 15
(el fill más caro), y sin un solo deadline perdido durante un guardado de EEPROM.
Confirmado a oído: el pop desapareció.

Esto además **saca del backlog subir UART7 a 921600** — ya no hace falta. Y es lo
que vuelve al instrumento apto para una instalación: con un pop por segundo no lo
era.

### 🔴 El presupuesto de latencia real — completo (2026-08-16)

Leyendo los datasheets del AK5538 y del PCM1690 aparecieron los dos términos que
faltaban. **El `Total pipeline` que imprime el heartbeat subestima la latencia
real en ~1 ms**, porque sólo cuenta buffer + fill:

| término | valor | ¿se puede bajar? |
|---|---|---|
| **Buffer (480 muestras)** | **10000 µs** | ✅ **sí — es el 88 %** |
| Fill | 668 µs | ya optimizado (era 1493) |
| Group delay del ADC (AK5538) | 396 → **104 µs** | ✅ hecho (`R04=0x02`) |
| Group delay del DAC (PCM1690) | **583 µs** | ❌ **fijo** |
| **TOTAL real** | **11,6 → 11,4 ms** | |

**El DAC no se puede mejorar**: el PCM1690 da 28/fs de group delay con nuestro
SCKI de 512 fs. La opción de 19/fs existe sólo con SCKI de 128 fs o 192 fs, y
esas frecuencias romperían el modo single-rate de 8 canales que necesitamos
(ver [pcm1690_ref.md](pcm1690_ref.md) y el descubrimiento #26 del histórico).
**583 µs es un piso del hardware.**

Por lo tanto **la única palanca que queda es el buffer**, que son 10 de los
11,4 ms. Ver la sección siguiente.

### Bajar el buffer de 480 a 240 muestras — la optimización grande pendiente

Ahorraría **5 ms**: la latencia total pasaría de 11,4 ms a **6,4 ms (−44 %)**.

**Ahora es viable**: con la CPU al 26,6 % hay lugar para el overhead extra de
duplicar la tasa de callbacks (estimado: subiría a ~30 %). Con el 59 % de antes
no lo era.

**Prerrequisitos: ✅ RESUELTOS (2026-08-16).** El deadline de fill pasa de 10 ms a
**5 ms**, así que hubo que auditar todo lo que bloquea el main loop. Aparecieron
**dos** operaciones que no entraban, no una:

| operación | bloqueo | ¿entraba en 10 ms? | ¿en 5 ms? | estado |
|---|---|---|---|---|
| `UART_Print` (heartbeat) | ~20 ms | ❌ | ❌ | ✅ resuelto 2026-08-14 |
| `EEPROM_SaveConfig` | ~12 ms | ❌ | ❌ | ✅ resuelto 2026-08-14 |
| `UART_Protocol_SendStatus` | ~5,2 ms | ✅ justo | ❌ | ✅ **resuelto ahora** |
| **`Apply_Master_Volume`** | **~9,6 ms** | ⚠️ **al borde** | ❌ | ✅ **resuelto ahora** |
| `send_ack` / `send_nack` | 434 µs | ✅ | ✅ | se deja bloqueante (ver abajo) |
| `Cal_Process` (AUTOFOCUS) | troceado | ✅ | ✅ | ya era cooperativo |

> 🔴 **`Apply_Master_Volume` era un hallazgo no previsto**: hace **32 escrituras
> I²C** (4 DACs × 8 canales) a 100 kHz ≈ 300 µs cada una = **~9,6 ms**. Estaba
> justo en el borde del deadline actual, o sea que **cada cambio de volumen ya
> venía metiendo un pop** en los 32 canales — con el buffer de 480. No se había
> detectado porque los cambios de volumen son esporádicos y el pop se confundía
> con el del heartbeat.

Los ACK/NACK **se dejan bloqueantes a propósito**: se mandan desde el parser, que
corre dentro de `HAL_UART_RxCpltCallback` — **contexto de ISR**, donde no se puede
servir el pipeline (correr un fill de 668 µs dentro de una interrupción bloquearía
todo lo demás). Son 5 bytes = 434 µs, que entra de sobra en ambos deadlines.

El mecanismo es el mismo en los cuatro casos: **usar el tiempo muerto en vez de
acortarlo**, vía hooks `__weak` (`EEPROM_Yield`, `UART_Protocol_Yield`) que `main.c`
redefine para llamar a `Service_Fills()`, o llamándola directamente donde el código
ya vive en `main.c`.

### Cómo hacer el cambio, cuando haya hardware

1. `AUDIO_BUFFER_SAMPLES` 480 → **240** en `main.c` (`ADC_BUFFER_SAMPLES` lo sigue
   automáticamente).
2. Verificar en el heartbeat: `Half`/`Full` pasan de 50/s a **100/s**, `CPU fills`
   sube de 26,6 % a ~30 % (estimado), `Err` sigue en 0.
3. `Undr` tiene que quedar en **0** — con `/status` en bucle y cambiando volumen,
   que son los dos casos recién arreglados.
4. La línea `Total pipeline` del log debería pasar de 10668 µs a **5668 µs**.

Si aparecen underruns que no se explican por los cuatro bloqueos de la tabla, el
sospechoso siguiente es el overhead de callbacks: al duplicar la tasa, el costo
fijo por callback se paga el doble.

### ❌ D-cache: SACADO DEL ROADMAP (2026-08-17)

> **Corrección.** Este documento venía diciendo que el D-cache era "el otro factor
> ~2-3 que queda sobre la mesa". **Es falso**, y AN4891 §5.2 lo dice explícito:
>
> > *"Enabling the cache allows the best performance to be reached for all memory
> > accesses **except for TCM-RAMs for which there is no effect on Cortex-M7 access
> > performance**."*
>
> **El D-cache no cachea la TCM.** Y desde que movimos `delay_line` a DTCM, los dos
> arrays calientes del pipeline —`delay_line` y `beam_line`— **ya viven ahí**. O sea
> que habilitar el D-cache **no tocaría ni una sola de las lecturas del lazo interno**.

Lo único que quedaría por ganar son las **escrituras a `audio_buffer*` en RAM_D2** —
que son justo los buffers que el DMA toca, o sea los que exigirían mantenimiento de
caché en cada callback, con las erratas §2.1.1 (corrupción en write-through) y §2.1.5
(store tras invalidate sin barrera) encima.

**Riesgo alto, ganancia chica, sobre el único camino que no podemos permitirnos
romper.** Queda descartado, no pospuesto.

### ✅ Lo que AN4891 sí valida, y por una razón mejor que la que dimos

Mover `delay_line` a DTCM fue correcto, pero el argumento bueno no era la velocidad:

> *"For highest data storage access performance by the Cortex-M7, the best location
> of R/W data is DTCM-RAM, since it is accessed **without latency with deterministic
> accesses**. [...] The other RAM locations do not provide deterministic access when
> cache is enabled **while still showing similar performance access as DTCM-RAM**."*

O sea: con caché, las otras memorias rinden **parecido en promedio** — pero **sólo la
DTCM es determinista**. En un pipeline con deadline duro de 10 ms (o 5 ms tras bajar
el buffer), **el determinismo vale más que el promedio**: lo que nos rompe no es una
latencia media alta, es un peor caso ocasional que se pasa del deadline.

### 🔵 La palanca que sí queda: MDMA

AN4891 describe **nuestro caso de uso con todas las letras** (§5.1 y §5.2):

> *"Whenever possible, if the data is located in SRAM1, SRAM2 or SRAM3, the user can
> copy it into the DTCM-RAM **using the MDMA** so that it is processed afterward by
> the CPU with more determinism and better performance."*
>
> *"**Audio data frames** can be processed in DTCM-RAM by copying them from SRAM1,
> SRAM2, or SRAM3 to DTCM-RAM using MDMA."*

Hoy esa copia la hace la **CPU**: `Process_ADC_Half` mueve 480×8 muestras de
`audio_buffer_3b_rx` (RAM_D2, vía AXI) a `delay_line` (DTCM), 100 veces por segundo.
Son ~1,5 MB/s de copia desde memoria lenta, pagados en ciclos de CPU que podrían
estar haciendo beamforming.

Dos aplicaciones posibles, en orden de relación beneficio/riesgo:

1. **Entrada (más simple)**: reemplazar la copia de `Process_ADC_Half` por MDMA.
   ⚠️ No es una copia lineal — hace *deinterleave* (`rx_buf[i*8+ch] → delay_line[ch][…]`).
   El MDMA soporta transferencias 2D/strided, así que es viable, pero hay que
   configurarlo con cuidado y verificarlo.
2. **Salida (más ganancia, más costo de RAM)**: llenar en un buffer de staging en
   DTCM y que el MDMA lo copie a los `audio_buffer*` de RAM_D2. Eso dejaría el lazo
   de fill **enteramente en DTCM, lecturas y escrituras a 0 wait states**.
   ⚠️ Cuesta DTCM: 3840 muestras × 4 B = 15 KB por banco. Con ~68 KB libres entran
   los 4 (61 KB) muy justo, o se reutiliza un solo staging secuencialmente.

⚠️ **Gotcha documentado** (§5.2): *"In the case of concurrent accesses of MDMA and
Cortex-M7 to the DTCM-RAM, the application speed can decrease if the Cortex-M7 does
not have the highest priority access"* — se ajusta por `CM7_AHBSCR`. Hay que
contemplarlo desde el principio, no descubrirlo midiendo.

**Nada de esto se toca antes de que el buffer de 240 esté medido y estable.** Es la
optimización *siguiente*, no una alternativa.

---

## 6. Soak test de 19,6 horas (2026-08-14) — números reales

Log de UART7 tras **70511 s (19,6 h)** de operación continua en modo 7, sin tocar
nada. Acá están los valores exactos que no se pudieron leer el día de los cambios.

| métrica | antes (`-Og`, sin caché) | después | factor |
|---|---|---|---|
| `Fill` min/avg/max | 1492 / 1493 / 1531 µs | **668 / 668 / 670 µs** | **2,23×** |
| ciclos por muestra de salida | 186,7 | **83,6** | |
| `CPU fills` | 59,2% | **26,6%** | |
| `SYNC bancos` | 4 µs (2038 cyc) | **1 µs (839 cyc)** | |
| latencia total del pipeline | 11493 µs | **10668 µs** | −0,8 ms |
| `Jitter range` | 86 cyc (0 µs) | 580 cyc (**1 µs**) | |
| `Temp` | 35,1-37,5 °C | 38,4-39,8 °C | |
| underruns por 60 s | 277-320 | **156-180** | |

> ⚠️ **Corrección a la estimación previa.** Este documento decía "esperado
> 10-15%". El valor real es **26,6%**. La ganancia fue **2,23×**, no los 4-6× que
> sugería esa estimación. No cambia ninguna conclusión —el modo 15 entra con
> margen— pero el número a usar de acá en adelante es 26,6%.

### Lo importante no es la velocidad: es la estabilidad

- **`Err: 0` a lo largo de 1156 ciclos de 60 s (19,3 h).** Cero errores de SAI.
- **Temperatura estable** en 38-40 °C durante todo el soak. Sube ~3 °C respecto de
  antes de los cambios, y está a 85 °C del máximo del H743. Irrelevante.
- **`SYNC bancos` constante** en 839 ± 6 ciclos las 19 horas (un solo outlier de
  1179). El sincronismo entre bancos no deriva con el tiempo ni con la temperatura.
- **`RXdma=100/s` y `OVF=0`** sostenidos: el AK5538 capturó sin interrupción.
- **Link al ESP32 vivo** todo el soak (155 KB acumulados de tramas de status).

Es un soak test aprobado. Es la evidencia que hacía falta antes de pensar en una
instalación que tenga que quedar prendida sola.

El `SYNC` mejoró de 4 µs a 1 µs como efecto secundario: con el código más rápido,
los cuatro `HAL_SAI_Transmit_DMA` del arranque caen más juntos todavía.

El jitter subió de 0 a 1 µs. Sobre un período de 20 ms eso es 0,005% — está en el
ruido de la medición, no es una regresión que importe.

### Underruns residuales — confirmada la causa A

La firma es exactamente la predicha por los prints bloqueantes: **+2 por segundo**,
y **+8 en el segundo donde cae el bloque `verbose`** (cada 10 s). Antes eran +4 y
+10; bajó porque con los fills más rápidos el main loop recupera más rápido tras el
bloqueo. Sigue sin ser un problema del pipeline de audio.

---

## 7. El buffer de 240 destapó dos bugs en el modo 14 (2026-08-18)

Bajar el buffer de 480 a 240 muestras salió limpio en el modo 0 —`TEST PASSED`,
0 underruns, latencia total de 10121 a **5054 µs**— y con eso se dio por buena.
**No alcanzaba: el modo 0 es el más barato de todos.** Probando modo por modo
después, el 14 estaba roto de dos maneras distintas.

Ambos bugs son de la misma familia: **el buffer no es sólo un número de latencia,
es el período que aparece en varios invariantes del pipeline.**

### 7.1 La tabla de senos estaba en FLASH

**Síntoma**: en modo 14 el `/status` daba `timeout` y se acumulaban **1234
underruns en 20 segundos** (~15% de los deadlines). El modo 15 —que es *más*
pesado: 8 fuentes contra 4— no perdía **ninguno**.

Que el modo más pesado ande y el más liviano no, descarta el presupuesto de CPU
como explicación. La diferencia está en **de qué memoria lee cada uno**:

```
$ arm-none-eabi-nm build/ds11_beamforming.elf | grep -E "delay_line|sine_table"
20006e00 b delay_line          ← DTCM, 0 wait states
08011aa0 r sine_table_1khz     ← FLASH, 4 wait states, y el D-cache está apagado
```

El modo 15 lee `delay_line` desde DTCM. El modo 14 lee `sine_table_1khz` **dos
veces por (muestra × canal × fuente)** = 61440 lecturas a flash por ventana de
5 ms. Los modos 12/13 usan la misma tabla pero una sola vez por muestra (240 por
fill), por eso nunca sufrieron.

**Es la misma causa raíz que el arreglo de agosto**: en este chip, con las cachés
de datos apagadas, lo que domina no es cuántas operaciones hacés sino de qué
memoria leés.

**Fix**: sacar el `const` **y** forzar la sección.

```c
__attribute__((section(".data")))
static int32_t sine_table_1khz[48] = { ... };
```

Sacar el `const` solo **no alcanza**: con `-O2` GCC ve que nadie escribe la tabla
y la manda a `.rodata` igual. Con la sección explícita cae en `.data`, que este
linker script ubica en DTCMRAM y el startup copia solo (+192 B de `.data`).

**Resultado: 1234 → 0 underruns**, y `/status` vuelve a responder dentro del modo.

> **Cómo verificarlo** (vale para cualquier tabla que se lea en un lazo de fill):
> `arm-none-eabi-nm build/ds11_beamforming.elf | grep <tabla>` tiene que dar
> `0x2000xxxx` y `d`/`b`, no `0x08xxxxxx` y `r`.

### 7.2 Tres de los cuatro tonos quedaron en contrafase

`mf_frame` lo avanzaba el fill de **SAI1_A**, o sea el primer banco, y los otros
tres lo leían **ya adelantado un buffer entero**. Eso era inocuo sólo si
`f × buffer / 48000` daba entero:

| buffer | condición sobre f | 1300 | 1600 | 1900 | 2100 |
|---|---|---|---|---|---|
| 480 | múltiplo de **100** | ✅ 13 | ✅ 16 | ✅ 19 | ✅ 21 |
| 240 | múltiplo de **200** | ❌ 6,5 | ✅ 8 | ❌ 9,5 | ❌ 10,5 |

Con el buffer de 240, **las filas 2-3 quedaron en contrafase con las 0-1 para 3 de
los 4 tonos**: en el punto focal se cancelaban en vez de sumarse — el foco de esos
tonos no estaba débil, estaba *invertido*. No lo detecta ninguna telemetría; era
audible y nada más.

**Fix**: avanzar `mf_frame` en el **último** banco (`ch_offset == AUDIO_CHANNELS*3`)
en vez del primero. Así los cuatro leen el mismo valor, rinden la misma ventana con
la misma fase, y la restricción sobre la frecuencia **desaparece** en vez de
cambiar de número.

### La lección de método

Validar un cambio de buffer **en el modo más barato no valida nada**. La regla para
la próxima vez que se toque `AUDIO_BUFFER_SAMPLES`: recorrer los modos 0, 12, 14 y
15 midiendo el delta de `underruns` en una ventana fija, y revisar a mano todo
invariante que mencione el tamaño del buffer (`grep -n "480\|buffer" main.c`).

---

## 8. El resumen de 60 s tiraba fills pendientes (2026-08-18)

Encontrado probando por telemetría, sin escuchar. El bloque que imprime el
resumen cada 60 s hacía, además de resetear estadísticas:

```c
__disable_irq();
need_fill_first_half = 0;
need_fill_second_half = 0;
...                          // las 8 banderas de TX
rx_3b_first_half_ready = 0;  // y las 2 del RX
rx_3b_second_half_ready = 0;
```

**Eso no es resetear una estadística: es tirar trabajo pendiente.** Una bandera
puesta significa "este medio buffer todavía no se llenó". Borrarla sin llenarlo
deja que el DMA reproduzca el contenido anterior — **medio buffer repetido en los
32 canales, o sea un pop, una vez por minuto**.

Encaja con el *"pedito cada vez que se imprime el serial"* que se venía
reportando. Los prints ya se habían hecho cooperativos en agosto y el síntoma
mejoró, pero **esto seguía descartando fills** y nadie lo miró: estaba en el
bloque de reseteo, donde uno espera ver estadísticas, no estado del pipeline.

Dejar las banderas puestas es estrictamente mejor: el fill se sirve **tarde**
(dato correcto, posición corrida) en vez de **no servirse** (dato viejo
garantizado).

### El contador de underruns mentía sobre su propia naturaleza

El mismo bloque hacía `underrun_count = 0`, así que `/status` publicaba un número
que **parecía acumulativo y retrocedía solo cada minuto**. Medir por diferencia
daba resultados imposibles —incluidos negativos— y llegó a parecer que la placa
se reiniciaba.

Ahora hay dos contadores: `underrun_count` por ciclo (lo que informa el resumen)
y `underrun_total`, que no se resetea nunca y es el que va a la telemetría. Ídem
`sai_errors`.

> **`uptime_ms` era el del ESP32, no el del STM32.** Un reinicio del STM32 era
> **invisible desde la red**: la única pista habría sido justamente el contador
> retrocediendo, que resultó ser otra cosa. Desde proto v5, `/status` publica
> `stm32_uptime_ms` (del STM32) junto a `uptime_ms` (del ESP32). Sin ese campo no
> se podía distinguir "reinicio" de "contador reseteado", y se perdió un rato
> largo en esa ambigüedad.

### Barrido completo de los 16 modos

Con todo lo anterior aplicado, con los amps apagados y sin señal de entrada:

| | resultado |
|---|---|
| Modos 0–15, ventana de 8 s cada uno | **0 underruns, 0 errores SAI, 0 clip** en los 16 |
| Barrido de volumen (10 cambios × 32 escrituras I2C) | **0 underruns** |
| Ganancia de entrada + `/status` concurrente | **0 underruns** |
| Ráfaga de beam custom (8 fuentes) | **0 underruns** |
| Cambios de modo rápidos (18 en 7 s) | 6 underruns — ver abajo |

Los 6 de la ráfaga de modos aparecen **una sola vez** y no se repiten en las
vueltas siguientes; con el arreglo del descarte de fills hay que volver a
medirlos. Falta la validación auditiva, que es la única que ve los artefactos
que no cuentan como deadline perdido.
