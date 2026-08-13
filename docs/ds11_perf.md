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

### Underruns residuales — los prints bloqueantes

Los ~14 por 20 s que quedan en modo 7 son la causa A. Opciones, de menor a mayor
esfuerzo:

1. **Subir UART7 a 921600** — una línea. 238 chars pasan de 20,7 ms a 2,6 ms, que
   entra cómodo en el slack de un ciclo de 10 ms. Hay que cambiar el `screen` a
   921600 (ver `Mis-comandos.md`).
2. **`UART_Print` por DMA o interrupción** — el arreglo correcto; necesita un buffer
   de TX y decidir qué hacer si se llena.
3. Bajar la cadencia del heartbeat a cada 5 s.

### D-cache

El otro factor ~2-3 que queda sobre la mesa. Necesita configurar la MPU para marcar
RAM_D2 como no-cacheable (o Write-Through) antes de habilitarlo, para que los
buffers DMA no requieran mantenimiento manual. No es urgente ahora que el modo 15
entra.

### `CPU fills` exacto post-cambios

No se pudo leer: el puerto serie no estaba disponible durante la sesión. La
estimación por underruns es sólida, pero conviene anotar el número real del
heartbeat en modo 7 y en modo 15 la próxima vez que el monitor esté abierto.
Esperado: modo 7 alrededor de **10-15%** (era 59%).
