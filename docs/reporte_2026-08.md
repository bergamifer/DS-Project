# Reporte — El bring-up de la DS11 (agosto 2026)

> Escrito 2026-08-14 para lectura posterior. Cubre desde la llegada de la placa hasta el
> soak test de 19,6 horas. Continúa [reporte_2026-07.md](reporte_2026-07.md). Los detalles
> técnicos viven en los docs enlazados; esto es el mapa y el registro de por qué se
> decidió cada cosa.

---

## 1. Dónde estábamos al arrancar agosto

La **DS11 llegó el 2026-08-01**: primera placa de todo el proyecto con los **4 PCM1690
poblados**, o sea **32 parlantes** en una matriz de 4×8. Hasta ahí todo el beamforming se
había validado sobre DS10 con **8 parlantes en una sola fila** — un array lineal que sólo
podía enfocar en X.

El salto no es de cantidad. Con 4 filas hay **480 mm de apertura vertical** además de los
600 mm horizontales, así que el instrumento pasa de ser una línea a ser un **array 2D
real**. Todo el código de steering ya estaba escrito para 32 canales, pero nunca se había
ejecutado contra hardware que los tuviera.

---

## 2. La placa no arrancaba — dos cortos de fábrica

El primer síntoma fue desconcertante: al enchufar el ST-Link, **su LED rojo se apagaba**.
Y el ESP32 tampoco se dejaba flashear. Medición: **+3V3 a GND = 0,3 Ω**. Un corto franco.

Lo que hizo el diagnóstico difícil es que **todas las placas de la tirada medían igual**,
lo cual descartaba un accidente de manipulación y apuntaba al diseño o a la fabricación.

El DRC de KiCad lo encontró, pero recién al correrlo de una forma específica:

```bash
kicad-cli pcb drc --format json --output drc.json          DS.kicad_pcb
kicad-cli pcb drc --format json --output drc_refill.json --refill-zones DS.kicad_pcb
# si el primero tiene violaciones que el segundo no tiene → los rellenos están viejos
```

**Causa raíz común: los gerbers se plotearon con los rellenos de zona desactualizados.**
KiCad usó el cobre almacenado en el archivo, anterior a las vias de stitching y las pistas
que se agregaron en DS11 — así que los planos nunca abrieron clearance alrededor de esos
objetos nuevos. El diseño estaba bien; **el ploteo no**.

1. **Via de GND cortocircuitando +3V3** en (136.815, 36.619), esquina de J3/USB-C. Una via
   de stitching suelta atravesando el relleno de `+3V3` en la capa interna **In2** con
   0,0000 mm de clearance. Invisible con lupa porque el corte está en capa interna. Se
   confirmó en el **drill de producción** (línea 660) que la via efectivamente se fabricó.
   Reparación: taladrarla con mecha de 0,9-1,0 mm.
2. **Pistas de +5VA sobre cobre de GND** bajo **C290**, en el riel de los amplificadores.
   Mismo mecanismo pero en la cara superior. Se removió C290 para abrir la pista.

**Regla de proceso que dejó** (ahora obligatoria antes de fabricar): *Refill all zones*
(tecla `B`) inmediatamente antes de plotear, y correr el DRC con y sin `--refill-zones`
comparando. El warning *"zone fills are out-of-date"* **no es cosmético**: costó una
tirada entera. Detalle completo en [ds11_errata.md](ds11_errata.md).

### Consecuencia a monitorear

Con C290 fuera, el riel `+5VA` de los 32 PAM8302A quedó con **660 µF de bulk en vez de
880 µF**. Se cruza con el riesgo ya conocido del buck MP2315S al límite. Si aparece
distorsión sólo a volumen alto y con muchos canales, hay que repoblarlo.

---

## 3. El falso corto: J7 pin 1

Parte del misterio inicial no era un corto sino un error de esquemático. El **pin 1 de J7
va directo al riel `+3V3`**, mientras que el pin 10 (5 V) sí pasa por el Schottky **D11**.
Sin esa protección, el ST-Link intenta alimentar toda la placa —STM32H743 + 4 PCM1690 +
AK5538 + ESP32, unos **700 mA**—, no puede, y se apaga.

O sea: **el LED apagándose no era evidencia del corto**, era un segundo problema
independiente que apuntaba en la misma dirección. Mitigación: cortar el pin 1 en el cable.
Para DS12 quedó anotado poner un Schottky en serie.

---

## 4. Los reinicios al arrancar los amplificadores

Con la placa ya reparada, al lanzar el channel walker se escuchó *"el primer tono con un
fade in y después se apagó"*. La telemetría confirmó un **loop de reinicio**: la secuencia
de `underruns` leída por HTTP era `0, 0, 6, 7, 0, 0, 5` — y los underruns se resetean en
cada boot.

La hipótesis inicial fue brown-out por el bulk reducido. La causa real resultó ser más
simple: **el límite de corriente de la fuente de banco**. Al subirlo, el walker recorrió
la matriz entera sin reiniciarse.

Vale la pena registrarlo porque el razonamiento correcto llevaba a un lugar equivocado:
había una explicación técnicamente sólida disponible (C290 removido → menos bulk → brown-out)
y encajaba con los síntomas. **La causa mundana ganó.** Ante cualquier reinicio con audio,
la fuente es lo primero a descartar.

### Mitigación que quedó igual, y por qué vale

Los **32 PAM8302A ahora arrancan apagados** (pin SD, PD3) y se habilitan por comando
(`CMD_SET_AMPS` / `POST /amps`). Aunque no era la causa, es una herramienta de diagnóstico
que separa limpio dos hipótesis que de otro modo se confunden:

- Estable con amps OFF y reinicia con ON → **es alimentación**.
- Reinicia igual con amps OFF → **no es alimentación**, buscar en otro lado.

Y evita el peor escenario: una placa en loop de reinicio que no se deja interrogar.

> ⚠️ **Riesgo de diseño que introduce**: el arranque de los amps depende de que el ESP32
> esté vivo y en red. Si el WiFi falla en una instalación, el instrumento queda mudo sin
> forma de recuperarlo. Pendiente de resolver (K2 con pulsación larga, o auto-enable tras
> N segundos sanos).

---

## 5. El sincronismo entre bancos: duda cerrada

Desde julio quedaba abierta la pregunta más incómoda del proyecto: **SAI1_A y SAI2_A son
masters asíncronos entre sí**. Comparten PLL3 (misma frecuencia, sin deriva), pero cada uno
arranca su stream al ponerle SAIEN y **ese offset queda fijo para siempre**. No se pueden
encadenar por SYNCEXT porque cada grupo de DACs recibe BCK/FS por pines propios.

Si el offset era grande, las filas 0-1 y 2-3 estarían reproduciendo instantes distintos de
la misma onda, y **el enfoque vertical no sería coherente** — justo la capacidad nueva de
DS11. Se temía hasta ~10 ms (3,4 m de camino acústico).

**Medido: 4 µs** al principio, **1 µs** después de las optimizaciones. Son **1,4 mm de
camino acústico**, contra un pitch de 80 mm entre parlantes. Irrelevante. Y **estable
arranque tras arranque** (839 ± 6 ciclos a lo largo de 19 horas), lo cual además significa
que una calibración de AUTOFOCUS **sí sobreviviría** un ciclo de alimentación.

El fix que lo logró fue mínimo: arrancar los cuatro `HAL_SAI_Transmit_DMA` **consecutivos,
sin ningún `UART_Print` en el medio** (un print de 41 caracteres a 115200 son ~3,6 ms de
bloqueo entre master y master).

---

## 6. El hallazgo grande: el M7 corría sin cachés

Con la placa estable, el modo 15 —MULTIFOCUS de 8 entradas, el objetivo artístico del
instrumento— **no funcionaba**. Medido por tasa de underruns: perdía el **55% de los
deadlines de fill**. El fill pedía más del 100% del tiempo disponible.

Mirando el costo real:

> `Fill avg = 716862` ciclos para 480 muestras × 8 canales = **186 ciclos por muestra de
> salida**, para un lazo interno de ~30 instrucciones.

Un factor 6 de penalidad inexplicable por el algoritmo. La causa: **no había un solo
`SCB_EnableICache()` en todo el proyecto**. El Cortex-M7 a 480 MHz venía buscando **cada
instrucción desde flash con 4 wait states** (`FLASH_LATENCY_4`). El core estaba al ralentí
esperando la memoria.

Tres cambios, ninguno tocando el algoritmo de beamforming:

1. **`SCB_EnableICache()`**. Sin riesgo de coherencia (el código no se automodifica) y sin
   timings dependientes de la velocidad de ejecución — se verificó que `pcm1690.c` usa sólo
   `HAL_Delay` (SysTick) y que el bit-bang de MCLK de DS9 ya no existe en DS11.
2. **`delay_line` fuera de `.dma_buffer`** → DTCMRAM. **Nunca fue memoria DMA**: el DMA del
   ADC escribe en `audio_buffer_3b_rx` y es la CPU la que copia. Estaba en RAM_D2 (vía AXI)
   sin necesitarlo, siendo la estructura más leída del pipeline.
3. **`OPT = -Og` → `-O2`**. El binario incluso quedó más chico.

**El D-cache sigue apagado a propósito**: habilitarlo obligaría a mantenimiento de caché
sobre los buffers DMA en cada callback, y el intento anterior terminó en HardFault. Queda
como trabajo futuro, con MPU marcando RAM_D2 como no-cacheable.

### Resultado

Underruns en 20 s (máximo teórico 8000 = todos los deadlines perdidos):

| modo | `-Og`, sin caché | + I-cache + DTCM | + `-O2` |
|---|---|---|---|
| 7 — TX focus | 100 | 48 | **14** |
| 15 — MULTIFOCUS 8 entradas | 4407 | 2189 | **102** |

**El modo 15 pasó de inusable a ~1,3% de deadlines perdidos.** Análisis completo en
[ds11_perf.md](ds11_perf.md).

---

## 7. El soak de 19,6 horas

Tras 70511 s de operación continua, los números reales del heartbeat:

| métrica | antes | después |
|---|---|---|
| `Fill avg` | 1493 µs | **668 µs** (2,23×) |
| `CPU fills` | 59,2% | **26,6%** |
| latencia total | 11,49 ms | **10,67 ms** |
| `Err` en 19,3 h | — | **0** |
| `Temp` | 35-37 °C | 38-40 °C, estable |

> **Corrección registrada**: se había estimado "10-15% de CPU". El valor real es **26,6%**
> y la ganancia fue **2,23×**, no los 4-6× que sugería esa estimación. No cambia ninguna
> conclusión, pero el número a usar es 26,6%.

Lo más valioso no es la velocidad: es **`Err: 0` a lo largo de 1156 ciclos de 60 s**, con
temperatura estable, SYNC sin deriva, ADC y link al ESP32 vivos todo el tiempo. Es la
evidencia que hacía falta para pensar en una instalación que quede prendida sola.

---

## 8. Estado al cerrar el reporte

### ✅ Validado

- Placa reparada, arranca y corre estable 19,6 h sin un solo error de SAI.
- Los 4 bancos SAI sincronizados a 1 µs, sin deriva.
- Presupuesto de CPU con margen: 26,6% en modo 7, y el modo 15 entra.
- Control completo por HTTP: modo, volumen, foco TX, foco por fuente, amps.
- Walker recorriendo los 32 canales.

### ⬜ Pendiente

- **Prueba de escucha del `-O2`** ← el único bloqueo real. El cambio de nivel de
  optimización puede destapar bugs latentes de `volatile` entre ISR y main loop, y **la
  telemetría no lo detecta**. Hay que escuchar walker, tono enfocado, monitor ADC (que es
  el que ejercita `delay_line`, la memoria que se movió) y multifoco.
- **`CPU fills` real en modo 15** — el único número que sigue estimado (~55-60%).
- **Inventario del walker**: cuáles de los 32 no suenan (quedaban parlantes desoldados).
- **Foco vertical (fy)** — la capacidad nueva de DS11, nunca probada acústicamente.
- Reparar los dos cortos en el resto de las placas de la tirada.
- Underruns residuales: subir UART7 a 921600 o pasar `UART_Print` a DMA.
- Arranque de amps sin depender del ESP32/WiFi.

---

## 9. Lo que este ciclo enseñó

**El warning que se ignora cuesta una tirada.** "Zone fills are out-of-date" estaba ahí,
visible, en cada ploteo.

**Una explicación sólida no es una causa demostrada.** El brown-out por bulk reducido
encajaba perfecto con los reinicios, tenía mecanismo físico y evidencia circunstancial. Era
el límite de corriente de la fuente.

**Vale la pena mirar el costo por muestra, no sólo el porcentaje.** El `CPU fills: 59%` se
puede leer como "vamos bien, hay margen". Los **186 ciclos por muestra para 30
instrucciones** son los que gritan que algo está mal — y llevaron a un 43× en el modo que
importaba.
