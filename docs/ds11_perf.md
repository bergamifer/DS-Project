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

### ⚠️ Prueba de escucha del `-O2` — obligatoria antes de confiar

Cambiar el nivel de optimización puede destapar bugs latentes de `volatile` en
código compartido entre ISR y main loop. La telemetría dice que está bien
(`sai_errors: 0`, el pipeline corre, los modos responden), pero **eso no prueba que
el audio sea correcto**. Hay que escuchar:

1. Modo 1 (walker): los 32 canales, uno por uno, limpio.
2. Modo 12/14: tono sin distorsión ni artefactos.
3. Modo 15: con una fuente conectada, que la voz esté situada y sin glitches.

Si algo suena mal, revertir es una línea (`OPT = -Og` en `firmware/Makefile`) y el
problema queda acotado a un bug de `volatile` que vale la pena encontrar igual.

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

### D-cache

El otro factor ~2-3 que queda sobre la mesa. Necesita configurar la MPU para marcar
RAM_D2 como no-cacheable (o Write-Through) antes de habilitarlo, para que los
buffers DMA no requieran mantenimiento manual. No es urgente ahora que el modo 15
entra.

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
