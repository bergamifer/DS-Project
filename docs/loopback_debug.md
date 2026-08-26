# Debug eléctrico por lazo cerrado (salida → entrada)

> Análisis pedido el 2026-08-20: *"¿podemos conectar una salida a la entrada
> desde J13 y analizar fase y nivel?"*
>
> **Respuesta corta: la idea es buena y J13 sirve a medias.** Su pin 1 llega por
> cobre a la salida del DAC del canal 24 (antes del amplificador); el pin 2 está
> suelto. Alcanza para medir un canal; para lo que más valor tiene —el skew entre
> los cuatro bancos— hacen falta los adaptadores de la §4.

---

## 1. J13: el pin 1 SÍ sirve, el pin 2 no

> **⚠️ Corrección del 2026-08-21.** La primera versión de este documento decía
> que J13 no estaba conectado a nada. **Estaba mal.** Fernando corrigió que el
> conector existe en la placa (sin poblar) y al rehacer el análisis apareció el
> error: mi primer barrido de cobre usaba una expresión regular que exigía capa
> y red en un orden fijo dentro de cada `(segment ...)`, y **se salteaba los
> segmentos que no matcheaban ese formato**. Un rebarrido exhaustivo —incluyendo
> vías y recorriendo la isla de cobre nodo por nodo— encontró 16 segmentos que
> el primero no había visto. La conclusión se invierte.

### Lo que hay realmente

| | |
|---|---|
| **Pad 1** | **16 segmentos de cobre en F.Cu**, que llegan hasta **R82** (1 kΩ, SMD, **poblada**) y de ahí a los pines VOUT1± de **U4 (PCM1690)** |
| **Pad 2** | sin cobre |

**U4 es DAC3 = canales lógicos 24-31** (`SAI2_B`, ver `pin_map.md`), así que
`VOUT1` de ese chip es el **canal 24**.

O sea: **J13 pin 1 es una toma de la salida del DAC del canal 24, antes del
amplificador.** Es exactamente la señal que uno querría medir: nivel de línea,
limpia, sin la portadora de clase D.

### Por qué no aparecía en el netlist

Todo ese cobre tiene **`net -1`** — sin red asignada. Se dibujó en el editor de
PCB pero nunca quedó ligado al netlist del esquemático, y por eso `DS.txt` no
menciona J13. El footprint además está marcado `dnp` + `exclude_from_bom` +
`exclude_from_pos_files`: la fábrica no soldó el header, pero **el cobre está**.

### Cómo se usa

1. Soldar un header de 2 pines (o directamente un cable) en J13.
2. **Pin 1 = señal.** El pin 2 no sirve: hay que tomar masa de otro lado (un
   punto de GND cercano, o el blindaje del RJ45).
3. Entra a una entrada del ADC: pata + al pin 1, pata − a masa.

Queda una medición **single-ended** de una de las dos patas del par
diferencial. Se pierden 6 dB de nivel y el rechazo de modo común, pero para
medir **retardo, polaridad y respuesta** eso no importa: son medidas relativas.

⚠️ **Antes de soldar, confirmalo con un multímetro**: continuidad entre J13 pin 1
y un extremo de R82. Es lo que cierra la discusión, porque acá se está leyendo
un archivo de diseño, no la placa que tenés en la mano — y ya vimos en esta
misma placa que los dos pueden no coincidir (los cortos de fábrica de agosto
salieron justo de eso).

### Lo que sigue siendo cierto: los TP1-TP6 no sirven

Los seis test points **sí** están como decía la versión anterior: `dnp`, sin red
y **sin un solo segmento de cobre** — el rebarrido exhaustivo tampoco encontró
nada. Ésos son huellas sueltas de verdad.

Y J13 da **un** canal, el 24. Para lo que más valor tiene —medir el skew entre
los cuatro bancos— hacen falta cuatro tomas, así que el adaptador de la §4 sigue
siendo necesario para los otros tres.

---

## 2. La cadena de salida, medida contra el esquemático

Para saber dónde sí se puede pinchar hay que tener el camino completo. Por canal:

```
PCM1690 VOUTn+ / VOUTn−        salida DIFERENCIAL de línea (8 pares por DAC)
        │
        ├── 1 kΩ serie                    ← fija la ganancia del PAM8302A
        ├── 4,7 nF a masa                 ← pasabajos de 33,9 kHz (anti-RF)
        ├── 220 nF serie                  ← acople
        │
   PAM8302A  IN+ / IN−                    clase D, BTL, mono
        │
        ├── ferrite bead + 220 pF         ← filtro de salida de clase D
        │
   JSPK01…16   (2 parlantes por conector, salida FLOTANTE)
```

Confirmado leyendo el símbolo cacheado: el **PCM1690 tiene 8 salidas
diferenciales** (`VOUT1+/−` … `VOUT8+/−`, pines 29-44). 4 DAC × 8 = 32 canales,
cada uno diferencial de punta a punta.

### 2.1 ⚠️ Dos números que salen de acá y no son menores

**El pasaalto de entrada del amplificador está en ~723 Hz.**

```
fc = 1 / (2π · 1 kΩ · 220 nF) = 723 Hz
```

A 800 Hz —la frecuencia del **modo 17**, el barrido vertical— eso son **−2,6 dB
y ~42° de corrimiento de fase**. Es el mismo para los 32 canales, así que **no
rompe el beamforming** (un corrimiento común se cancela en los retardos
relativos). Con tolerancia de ±10 % en los capacitores, la dispersión entre
canales a 800 Hz es de ±2,8°, o sea ~10 µs ≈ 3,3 mm de camino acústico:
despreciable frente a los 80 mm de separación. Pero conviene saber que **el modo
17 trabaja a media octava del borde de la banda**, no en el medio.

**La ganancia del PAM8302A es de +49 dB.**

```
A_V = 2 · (142 kΩ / R_I) = 2 · (142k / 1k) = 284  →  +49 dB
```

Eso es muchísimo, y tiene una consecuencia directa sobre cómo hay que operar el
equipo — la sección que sigue.

---

## 3. ⚠️ El volumen de fábrica está unos 16 dB demasiado alto

Encadenando los números del esquemático:

| | valor |
|---|---|
| Salida del PAM8302A antes de recortar (5 V, BTL, 4 Ω, 2,5 W) | ~3,2 V<sub>rms</sub> |
| Ganancia del amplificador | ×284 |
| **Entrada máxima sin recortar** | **~11 mV<sub>rms</sub>** |
| Fondo de escala diferencial del PCM1690 | ~1,4 V<sub>rms</sub> |
| **Atenuación digital necesaria** | **≈ −42 dB** |

Con los modos de tono, que emiten a −6 dBFS:

| volumen | entrada al amp | salida pedida | resultado |
|---|---|---|---|
| **−24 dB** (default de fábrica) | 44 mV | 12,6 V<sub>rms</sub> | **12 dB dentro del recorte** |
| **−40 dB** | 7,0 mV | 2,0 V<sub>rms</sub> | limpio, con margen |
| **−46 dB** | 3,5 mV | 1,0 V<sub>rms</sub> | limpio, ~10 dB de margen |

**Fernando venía operando el equipo a −46 dB por oído.** La cuenta dice −42 dB.
Que una derivación de esquemático y una elección auditiva independiente caigan a
4 dB una de otra es la mejor corroboración disponible sin instrumental.

> **Recomendación**: el default de fábrica debería ser **−40 dB**, no −24 dB.
> Es un cambio de una línea (`Config_LoadDefaults`), y hace que el equipo
> arranque en un punto donde no distorsiona en vez de uno donde sí.
>
> Esto también explica de dónde salía el problema de estructura de ganancia de
> agosto: con el amplificador ya 12 dB adentro del recorte, subir la ganancia de
> entrada no podía terminar bien.

---

## 4. El lazo cerrado que SÍ se puede hacer

J13 da **un solo canal** (el 24) y hay que soldarle un header. Para los otros
tres bancos —y para medir la cadena COMPLETA, amplificador incluido, que J13 no
ve porque toma antes— **no hace falta tocar la placa**: las salidas de parlante
son conectores JST accesibles y las entradas son RJ45. Con un adaptador de seis
componentes se cierra el lazo.

> Las dos tomas son complementarias, no alternativas. **J13 mide el DAC**
> (limpio, sin clase D). **El adaptador mide todo**, incluido el amplificador y
> su pasaalto de 723 Hz. Comparar las dos sobre el mismo canal aislaría qué
> aporta cada etapa.

### 4.1 Por qué no se puede conectar directo

| | por qué |
|---|---|
| **Nivel** | la salida del amp llega a ~3,2 V<sub>rms</sub>; la entrada del ADC es de línea. Hay que atenuar ~20-25 dB |
| **Portadora de clase D** | el PAM8302A conmuta a ~250 kHz. El ferrite de salida ayuda pero no alcanza; el filtro de entrada del ADC (470 Ω + 100 pF) corta recién en 3,4 MHz. Sin pasabajos, eso entra al ADC y **se pliega dentro de la banda de audio** |
| **Salida flotante** | la salida BTL no tiene ninguna pata a masa |

Lo bueno: **la entrada del ADC ya es diferencial y con acople capacitivo**
(2,2 µF en serie por pata, más 470 Ω y protección ESD). Eso la hace compatible
con una fuente flotante — bloquea el modo común y sólo pasa la diferencia.

### 4.2 El adaptador

Por cada canal a medir, entre un par de salida de parlante y un par de entrada:

```
   SPK+ ──[ 10k ]──┬──────────────── RJ45 pin (+)
                   │
                  [1k]     [10n]      ← en paralelo entre sí
                   │         │
   SPK− ──[ 10k ]──┴─────────┴─────── RJ45 pin (−)
```

- **División**: 1k / (10k + 1k + 10k) = **1/21 = −26 dB**. Con el amplificador
  a fondo (3,2 V) entrega 150 mV al ADC: bien arriba del piso de ruido (437
  cuentas) y lejos de saturarlo.
- **Pasabajos**: R equivalente = 1k ∥ 20k = 952 Ω; con 10 nF da **fc ≈ 16,7 kHz**.
  Atenúa la portadora de 250 kHz ~24 dB adicionales. A 2 kHz el corrimiento de
  fase es de 6,8°, **igual para todos los canales que se midan con el mismo
  adaptador**, así que se cancela en cualquier medición relativa.
- Resistencias al 1 % y capacitores NP0/C0G si se busca precisión de nivel; para
  fase y retardo la tolerancia no importa mientras los adaptadores sean iguales.

⚠️ **Empezar siempre con el volumen abajo** y subir mirando `adc_pk` en
`/status`. El divisor protege del nivel nominal, no de un error de conexionado.

---

## 5. Qué habilita esto — en orden de valor

### 5.1 🔴 Medir el SYNC entre bancos ELÉCTRICAMENTE

**Es la medición que más falta hace.** Hoy el número de 4 µs sale de comparar
marcas de `DWT->CYCCNT` en los callbacks de DMA: mide cuándo el *software* se
entera, no cuándo sale la señal. El offset entre los dos SAI master
(`SAI1_A` y `SAI2_A`) es un dato de hardware que se fija al arrancar el stream,
y hasta ahora nunca se midió del lado analógico.

Con **cuatro adaptadores** —un canal de cada banco (0, 8, 16, 24) a cuatro
entradas del ADC— se capturan los cuatro simultáneamente y por correlación
cruzada salen los tres offsets con precisión de sub-muestra.

Si el número analógico coincide con los 4 µs del firmware, el invariante queda
confirmado del lado que importa. Si no coincide, apareció algo que hoy es
invisible.

### 5.2 Verificar que los retardos fraccionales Q8 se aplican de verdad

El beamforming se apoya en retardos de sub-muestra (`tx_delay_q8`). Nunca se
comprobaron: se validan por sus consecuencias acústicas, que dependen también de
la sala. Dos canales del mismo banco con un foco conocido → la correlación
cruzada da el retardo real y se compara contra el que el firmware dice aplicar.

### 5.3 Mapeo de canales y polaridad, sin oído

El cruce JSPK09/JSPK10 y la doble inversión de los canales impares hoy se
verifican con el walker y el oído. Con el lazo cerrado, **qué índice lógico sale
por qué conector físico, y con qué signo**, es una medición de segundos.

### 5.4 Estructura de ganancia real

Cerrar la pregunta de la §3 con un número medido en vez de una derivación:
barrer el volumen de −60 a 0 dB y ver dónde la respuesta deja de ser lineal.
**Ese punto es el volumen máximo utilizable del instrumento**, y hoy no se sabe.

### 5.5 Respuesta en frecuencia de la cadena de salida

Confirmar el pasaalto de 723 Hz y ver qué pasa realmente a 800 Hz, que es donde
trabaja el modo 17.

---

## 6. Firmware que haría falta

Poco, porque **la maquinaria de correlación ya existe**: el AUTOFOCUS
(`Cal_CorrAt`, interpolación parabólica, refinamiento sub-muestra) hace
exactamente esta cuenta contra un chirp de referencia. Un modo LOOPBACK sería:

1. Emitir el chirp de `cal_chirp[]` por los canales elegidos, de a uno o a la vez.
2. Capturar las entradas del ADC (ya lo hace `Process_ADC_Half`).
3. Correlacionar → retardo sub-muestra, signo y nivel de pico.
4. Publicar por `/status` como hace el AUTOFOCUS.

La diferencia con el AUTOFOCUS es que **no hay sala en el medio**: sin eco ni
reverberación, la correlación es limpia y el resultado es del equipo, no del
ambiente.

---

## 7. Para la DS12

1. **Puntos de prueba que existan.** Los siete de la DS11 (J13 + TP1-6) están
   todos DNP y sin red. Antes de mandar a fabricar: verificar que cada TP tenga
   red asignada y cobre, no sólo footprint.
2. **Un header de lazo cerrado de fábrica**: un canal de cada banco (4 pares)
   llevado a un header, con el divisor y el pasabajos **en la placa**. Convierte
   todo lo de la §5 en "enchufar un cable plano" en vez de "armar adaptadores".
3. **Revisar la estructura de ganancia**: +49 dB de amplificador obligando a
   −42 dB de atenuación digital es tirar rango dinámico. Un `R_I` más alto
   (20 kΩ → +23 dB) subiría el punto de operación digital ~26 dB, con la mejora
   de relación señal-ruido que eso implica. **De paso corre el pasaalto de
   723 Hz a 36 Hz**, y el eje vertical dejaría de trabajar al borde de la banda.

---

## 8. RESULTADOS 2026-08-25 — primera medición eléctrica del lazo

Placa **nueva** (la segunda DS11, primera con los cuatro PCM1690 poblados y
respondiendo). J13 soldado y cableado a una entrada del RJ45. Sin 24 V: la placa
corre de USB, así que **no hay amplificador en el camino** — se mide DAC → ADC,
que es exactamente lo que J13 promete.

### Configuración y hallazgos de entrada

| | |
|---|---|
| Entrada real | **4** (no la 0, que era la estimación). El modo mide las ocho y lo descubrió solo |
| Polaridad | **+** (no invertida) |
| Volumen | **hace falta subirlo**. A −40 dB (el default correcto) el chirp no supera el umbral: `pico=1` en las ocho. A 0 dB, `q≈1130`. J13 toma antes del amplificador, así que la atenuación digital le pega de lleno |

> ⚠️ Volver a **−40 dB** al terminar. Con 24 V y amps encendidos, 0 dB mete al
> PAM8302A (A_V = +49 dB) unos 40 dB adentro del recorte.

### La latencia absoluta del lazo

**7313 – 7399 µs**, o sea ~351–355 muestras. Es la vuelta completa
TX DMA → PCM1690 → J13 → AK5538 → RX DMA.

### Repetibilidad de la MEDICIÓN: mejor que 81 ns

16 lecturas seguidas sin tocar nada dieron **el mismo valor hasta el último
dígito** (`t=355.17 smp`), mientras `q` variaba entre 1125 y 1149 — o sea que la
correlación se recalcula de verdad, y aun así el retardo no se mueve ni 1/256 de
muestra. El instrumento no es el límite de nada de lo que sigue.

### 🔴 La latencia NO es determinística entre arranques — pero es cuantizada

26 reinicios, midiendo una vez por arranque:

| estado | muestras | µs | veces |
|---|---|---|---|
| A | 351,02 | 7312,9 | 14 (54 %) |
| B | 354,04 | 7375,8 | 4 (15 %) |
| C | 355,17 | 7399,4 | 8 (31 %) |

- **Dispersión total: 4,17 muestras = 86,9 µs = 29,8 mm de camino acústico.**
- Dentro de cada estado, ±0,01 muestras (±0,2 µs).
- Saltos entre estados: 3,02 y 1,13 muestras. **No son múltiplos enteros** — el
  mecanismo todavía no está explicado.
- Sólo aparecieron **tres** estados en 26 arranques.

### Qué significa

**`multinode.md` §2 predecía "hasta ~medio buffer, ms" (2500 µs). El valor real es
87 µs: 29 veces menos.** Esa parte del análisis era demasiado pesimista.

**Pero 87 µs no es despreciable.** Dos nodos que arranquen en estados distintos
quedan con 30 mm de diferencia de camino, que a 2,1 kHz —el borde de la banda
espacial útil documentada— son **66° de error de fase**. Para un beam conjunto eso
degrada de verdad.

**La buena noticia es que sea cuantizado.** Un error discreto de tres estados es
mucho más fácil de atacar que una dispersión continua: sugiere una carrera con
tres desenlaces en el arranque, y el "arranque determinístico" (nivel 1 de
`multinode.md` §2 — arrancar RX primero y disparar los TX en su primer callback)
debería colapsarlo a un solo estado. **Y ahora hay con qué verificarlo**: este
mismo modo mide si funcionó.

### Alcance de esta medición — mejor de lo que parecía

> **Corrección del 2026-08-25.** La primera versión de esta sección decía que los
> reinicios por NRST no reseteaban los codecs y que por eso la dispersión real
> podía ser mayor. **Estaba mal**, y la pregunta de Fernando ("¿se pueden resetear
> el ADC y los DAC por software?") lo destapó.

Los 26 reinicios fueron por NRST vía SWD, pero eso **sí incluye un POR completo de
los codecs**: `main.c:3036` y `main.c:3042` bajan `DAC_RST` (PC11, compartido por
los cuatro PCM1690) y `AK_PDN` (PC7) como primera acción del init de GPIO, y los
liberan recién con los tiempos del datasheet (12 ms tras RST, 15 ms tras PDN).
El datasheet del AK5538 confirma que un ciclo de PDN devuelve **todos los
registros a default**.

O sea que cada uno de esos 26 arranques resetéo de verdad los cinco chips. **Los
tres estados son una propiedad real de la secuencia de arranque, no un artefacto
de haber reseteado en caliente.**

Lo único que un corte de alimentación agregaría por encima de esto: el colapso y
re-asentamiento de los rieles analógicos, el POR propio del STM32 (distinto de un
reset por pin) y el arranque en frío del cristal. Todo de segundo orden para esta
medición.

### Siguiente paso para explicar el mecanismo

Los saltos de 3,02 y 1,13 muestras no son múltiplos enteros y no hay explicación.
Como los group delays de PCM1690 y AK5538 son constantes de datasheet, lo que
varía tiene que estar **del lado del STM32**. El experimento que lo cierra es el
**nivel 2 de `multinode.md` §2**: registrar `NDTR` de los DMA de RX y TX en el
instante del chirp y ver si el estado correlaciona con la fase relativa de los
DMA. Es una adición chica de firmware y convierte la conjetura en un dato.

### Lo que esto NO midió

Sigue faltando el **skew entre los cuatro bancos**, que es lo que más valor tiene
(§5.1): J13 da un solo canal (el 24, banco 3), así que 1 de 8 entradas ve el
chirp y no hay contra qué comparar. Hacen falta los adaptadores de §4 en los otros
tres bancos — y ésos sí necesitan 24 V, porque toman de la salida de parlante.

---

## 9. RESUELTO 2026-08-25 — los tres estados son la fase de los DMA

§8 dejó abierto por qué la latencia cae en tres estados. Como los group delays de
PCM1690 y AK5538 son constantes de datasheet, la sospecha era la fase relativa
RX↔TX dentro del STM32. **Ahora es un dato, no una sospecha.**

### El instrumento

En el instante exacto de armar la captura (`main.c`, fill del banco 24, cuando
`lb_chirp_pos == 0`) se leen los dos contadores de DMA y se imprimen junto al
retardo:

```c
lb_ndtr_rx = __HAL_DMA_GET_COUNTER(&hdma_sai3_b);   /* RX del AK5538  */
lb_ndtr_tx = __HAL_DMA_GET_COUNTER(&hdma_sai2_b);   /* TX del banco 24 */
```

Los dos DMA están programados con la misma cuenta (3840 palabras = 480 muestras
de doble buffer), así que las posiciones son comparables sin escalar. `NDTR`
cuenta hacia abajo, de modo que la posición es `(3840 − NDTR) / 8` y la fase es
la resta, normalizada a `[0, 480)` porque es circular.

### El resultado: correlación total

20 arranques:

| RX−TX | veces | latencia (muestras) |
|---|---|---|
| ~78 | 6 | **351,14** |
| 81 | 8 | **354,13** |
| 82 | 6 | **355,29** |

**La fase de los DMA determina la latencia por completo.** Tres estados de fase,
tres estados de latencia, mapeo uno a uno, sin una sola excepción en 20 arranques.

Detalles que confirman la lectura:

- Dentro de un arranque la fase no se mueve: dos medidas consecutivas dieron
  `RX=80 TX=2` y `RX=320 TX=242` —medio buffer aparte— y **`RX−TX=78` en las dos**.
- Los valores 77 y 78 dan la **misma** latencia: la fase se lee con ±1 de ruido,
  porque los dos `NDTR` se leen con ~10 ciclos de diferencia y el DMA avanza en
  el medio. Son el mismo estado.
- 3 unidades de fase → 2,99 muestras de latencia. Uno a uno.

### Lo que esto habilita

**1. El nivel 1 de `multinode.md` §2 queda justificado y cuantificado.** Arrancar
los TX en el primer callback del RX fija la fase → un solo estado. La mejora
esperable:

| | dispersión entre nodos |
|---|---|
| hoy | 4,17 muestras = 87 µs = **29,8 mm** |
| dentro de un estado | 0,03 muestras = 0,6 µs = **0,21 mm** |

**140× menos.** Y ahora hay con qué verificar que el arreglo funcionó.

**2. El nivel 2 no necesita cable.** Éste es el hallazgo práctico grande: la fase
RX−TX se lee de un registro, en cualquier momento, **sin loopback, sin J13, sin
adaptador y sin micrófono**. El cable de J13 hizo falta para *descubrir* la
relación; no hace falta para *usarla*. En una instalación de 10-16 cajas, cada
nodo puede leer su propia fase al arrancar y saber en qué estado quedó —
compensable en el offset de la delay line, o al menos reportable por telemetría.

### Lo que queda abierto

El salto de fase 81→82 da 1,16 muestras de latencia en vez de 1,00, mientras que
78→81 da 2,99 sobre 3. Probablemente sea sesgo del ruido de lectura: leer los dos
`NDTR` con interrupciones deshabilitadas y sin nada en el medio debería cerrarlo.
No bloquea nada — el mapeo estado↔estado es unívoco igual.

---

## 10. ARREGLADO 2026-08-25 — arranque determinístico del RX

§9 dejó el mecanismo explicado: la fase RX↔TX de los DMA determina la latencia.
Acá está el arreglo, y la verificación de que funcionó.

### La causa

El TX no se puede mover: **SAI1_A genera el MCLK** que los cuatro PCM1690 y el
AK5538 necesitan para completar su POR, así que tiene que arrancar primero. Lo
que quedaba suelto era el RX, y arrancaba *"cuando el boot llegaba"* — después de
un `UART_Print` de ~3,3 ms bloqueantes y de una lectura de EEPROM **con ramas de
distinta duración según se detecte o no**. La fase terminaba dependiendo de
cuánto había tardado el camino de boot.

### El arreglo

Enganchar el arranque del RX a un borde conocido del TX. `NDTR` cuenta hacia
abajo y al recargarse sube, así que un `now > prev` es el wrap del buffer
circular — un borde que ocurre cada 10 ms y es idéntico en todos los arranques:

```c
uint32_t prev = __HAL_DMA_GET_COUNTER(&hdma_sai2_b);
for (;;) {
    uint32_t now = __HAL_DMA_GET_COUNTER(&hdma_sai2_b);
    if (now > prev) { rx_aligned = 1; break; }   /* wrapeó */
    prev = now;
    if ((HAL_GetTick() - t0) > 50u) break;       /* el TX no corre */
}
status = HAL_SAI_Receive_DMA(&hsai_BlockB3, ...);
```

El timeout existe para que un TX que no arrancó no cuelgue el boot: si pasa, se
arranca el RX igual (peor fase, pero equipo vivo) y **se reporta en el log** —
`fase SIN ALINEAR` — para que no sea un fallo silencioso.

### Verificación

| | arranques | dispersión |
|---|---|---|
| **antes** | 46 | 4,17 muestras = 87 µs = **29,8 mm** (tres estados) |
| **después** | 25 | 0,03 muestras = 0,6 µs = **0,21 mm** (un estado) |

**140× menos**, que es exactamente lo que predecía la dispersión intra-estado de
§9. La fase quedó en 0 en **41 de 41** arranques medidos (16 leyendo el heartbeat
+ 25 con el loopback).

> La latencia absoluta cambió de ~351 a ~513,67 muestras. Es esperable: se movió
> el instante en que arranca el RX, así que el lazo mide otra cosa. Lo que
> importa es que ahora **es la misma en todos los arranques**.

### Un artefacto del modo 19, que NO se arregló — **explicado en §13**

En 1 de 25 arranques el loopback informó `t=273,67` en vez de `513,67`:
**exactamente 240,00 muestras menos, o sea medio buffer justo.**

No es el pipeline. La evidencia: **ese arranque también midió fase 0**, y §9
estableció que la fase determina la latencia uno a uno. Si el pipeline hubiera
cambiado 240 muestras, la fase habría cambiado con él.

Lo más probable es una carrera en el armado de la captura: `lb_capturing = 1` se
pone desde el fill (contexto de main loop) y el callback del RX empieza a copiar
en la mitad siguiente. Si el armado cae muy cerca del borde de una mitad, la
captura arranca una mitad después → 240 muestras. **Queda anotado, sin arreglar**:
es un artefacto de un modo de banco, no afecta al equipo, y aparece 1 de cada 25.

> **La conjetura era correcta y quedó probada — §13.** El mecanismo es el que se
> sospechaba, con un matiz: no depende de caer "cerca del borde" sino de si la
> mitad del RX que acaba de terminar ya fue consumida cuando corre el fill.

---

## 11. Los retardos fraccionales Q8, verificados contra el mundo físico (2026-08-25)

El equipo dice tener **81,38 ns de resolución de retardo** (Q8 = 1/256 de muestra)
y eso **nunca se había verificado**. Si la interpolación estuviera sutilmente mal,
todos los beams estarían sutilmente mal y no hay ninguna bandera de error que lo
delate: es la matemática central del instrumento, sin auditar.

El loopback mide con esa misma resolución y su piso está por debajo de 81 ns
(§8), así que es el instrumento adecuado. Se le agregó al modo 19 un retardo
fraccional sobre el chirp, **con la misma fórmula del fill real** —
`s0 + ((s1-s0)*fr >> 8)`, no una copia equivalente: si se reimplementara, la
prueba verificaría la copia en vez del código que sale por los parlantes.

Se comanda por `POST /command` reinterpretando `beam_config.delays[]` como Q8
dentro de este modo (el loopback no lo usa para otra cosa). Con 0 —el default— el
comportamiento es idéntico al de siempre.

### Resultado — dos barridos, sin alimentación de 24 V

| Q8 | comandado | medido | error | error | error |
|---|---|---|---|---|---|
| 16 | 0,0625 | 0,060 | −0,002 | −0,05 µs | −0,02 mm |
| 32 | 0,1250 | 0,125 | +0,000 | 0,00 µs | 0,00 mm |
| 64 | 0,2500 | 0,240 | −0,010 | −0,21 µs | −0,07 mm |
| 128 | 0,5000 | 0,480 | **−0,020** | **−0,42 µs** | **−0,14 mm** |
| 192 | 0,7500 | 0,735 | −0,015 | −0,31 µs | −0,11 mm |
| 256 | 1,0000 | 0,990 | −0,010 | −0,21 µs | −0,07 mm |
| 512 | 2,0000 | 1,995 | −0,005 | −0,10 µs | −0,04 mm |

**Los retardos fraccionales funcionan.** Error máximo **0,020 muestras = 0,42 µs
= 0,14 mm = 0,31° a 2,1 kHz**, y los dos barridos reproducen a ≤0,03 muestras.

Un paso de **1/16 de muestra** (0,0625) se resolvió con 0,002 de error: el
steering sub-muestra es real y medible, no una cifra de papel.

### Y contesta una pregunta abierta del propio código

El comentario de `tx_delay_q8` dice: *"the first rung of the sub-sample ladder
(**polyphase FIR only if measurements ever demand it**)"*.

**Esta medición dice que no lo demandan.** La forma del error —cero en muestras
enteras, máximo en la mitad— es la firma exacta de la interpolación lineal, y su
peor caso son 0,14 mm contra 80 mm de separación entre parlantes. Un FIR
polifásico sería trabajo sin efecto audible ni medible.

---

## 12. OTA verificado en la segunda placa (2026-08-25)

Los dos chips se habían flasheado por cable, así que **el camino OTA nunca había
corrido en esta placa**. Para una instalación de 10-16 cajas es *el* mecanismo de
recuperación, y uno sin probar no es un mecanismo de recuperación.

`make ota OTA_HOST=ds-1e1f30.local` → 85548 bytes, CRC `63F143BB`, imagen
verificada, reinicio automático, bootloader aplicándola, cabecera de staging
borrada, firmware nuevo corriendo. **Funcionó de punta a punta.**

> ⚠️ `OTA_HOST` por defecto en el Makefile sigue siendo `domosonica.local`, que ya
> no resuelve. Hay que pasar `OTA_HOST=` hasta que se actualice el default.

### Una lección de método

Al verificarlo casi lo doy por roto. Leí `stm32_uptime_ms` después del OTA, vi
3658 s y concluí que no había reiniciado — cuando en realidad había reiniciado 68
minutos antes y yo había tardado eso en volver a mirar.

**Una lectura suelta de uptime no distingue "nunca reinició" de "reinició hace
rato".** Lo resolvió el registro continuo del soak, que muestrea cada 30 s:

```
14:55:17  uptime 2695s -> 34s    (flasheo por ST-Link)
15:41:40  uptime 2746s -> 43s    <- el OTA
16:49:46  uptime 4093s -> 13s    (reset manual)
```

Para detectar reinicios hay que **vigilar**, no consultar.

---

## 13. RESUELTO 2026-08-25 — el artefacto de 1 en 25 es la ventana, no el pipeline

§10 dejó una carrera anotada sin arreglar y sin probar. Acá está el instrumento,
la confirmación por observación, y la confirmación por causa.

### El instrumento

Tres registros nuevos, todos en el camino del modo 19, ninguno en el de audio:

| dónde | qué |
|---|---|
| al armar, **dentro** del mismo `__disable_irq()` que los NDTR | `lb_arm_pend` — qué mitades del RX estaban pendientes de procesar |
| al armar | `lb_arm_rxcb` — el contador de callbacks del RX |
| en el **primer** copiado de `Process_ADC_Half` | `lb_cap_half` — con qué mitad abrió la captura (sale del puntero), y el contador otra vez |

Las banderas se leen dentro del mismo bloqueo que los NDTR a propósito: si se
leyeran afuera, un borde de RX podría caer en el medio y la foto mezclaría dos
instantes — que es exactamente el error que se está buscando.

De ahí sale la línea nueva del informe:

```
  armado: pos=243 mitad=1 q=3 pend=1 cap=0 dRX=0 back=243 k=1
```

`back` es la distancia **medida** entre el armado y el primer sample capturado, y
`k` su parte en mitades: 0 si la captura abrió con la mitad que el DMA estaba
escribiendo, 1 si abrió con la anterior.

### Un normalizador que estaba mal, y cómo lo delató el propio dato

La primera versión imprimía `t − back`, suponiendo que había que descontar la
distancia entera. **Está mal**, y el dato lo dijo enseguida: `t − back` tenía
**3,13** muestras de dispersión contra **0,26** de `t` pelado. Restar `back`
agregaba ruido en vez de sacarlo.

La razón: el chirp sale en un borde de mitad y la ventana abre en un borde de
mitad, así que `t` está **cuantizado en pasos de 240** y `q` —dónde cayó el
armado *dentro* de la mitad— no entra. La prueba directa está en la tabla: sobre
382 informes `q` recorrió 0..26 y `t` no lo siguió en absoluto. Lo que hay que
descontar es `240k`, nada más.

> Vale anotarlo como método: **el normalizador equivocado se delata solo, porque
> empeora la dispersión.** Si hubiera pasado desapercibido, la conclusión habría
> sido "ni `t` ni `t − back` son estables" y la carrera habría quedado sin
> explicar por segunda vez.

### Confirmación 1 — por observación: 382 informes, 61 arranques

| | dispersión de `t` | dispersión de `t − 240k` |
|---|---|---|
| 382 informes | **240,19 muestras** (dos estados) | **0,26 muestras** (uno solo) |

Y el discriminador acierta **382 de 382**, sin una excepción:

| `pend` | `cap` | `dRX` | `k` | n | `t` |
|---|---|---|---|---|---|
| 0 | la que escribe | 1 | 0 | **1** | **273,72** |
| ≠0 | la anterior | 0 | 1 | 381 | 513,65 – 513,91 |

El caso raro apareció **1 vez en 382 informes**, del mismo orden que el 1 de 25
de §10, y midió **273,72** contra los 273,67 que §10 había anotado.

### Y el arranque raro dice *por qué*, en el log

El informe anómalo cayó pegado a la reimpresión del banner del modo:

```
MODE [20/20]: LOOPBACK electrico (bancos 0/8/16/24 -> ADC 0-3)
  armado: pos=26 mitad=0 q=26 pend=0 cap=0 dRX=1 back=26
  entrada5 (c4)  t=273.72 smp (5702 us)
```

`q=26` en vez de los 3 de siempre: el `UART_Print` del banner **atrasó el fill
del banco 24 unos 542 µs**, y para cuando corrió, la mitad del RX ya había sido
consumida por un `Service_Fills` intermedio. Sin nada pendiente, la captura abrió
con la mitad que el DMA estaba escribiendo → 240 muestras menos.

Eso corrige el matiz de §10: **no es "caer muy cerca del borde"**. El armado cae
*siempre* a 0-3 muestras del borde — es donde lo dejó el arreglo de §10. Lo que
decide es si algo se metió en el medio y le dio tiempo a la mitad pendiente a ser
consumida antes del fill.

### Confirmación 2 — por causa: el otro lado, a pedido

Esperar 1 de cada 382 no es una prueba, es una anécdota con suerte. Un build de
laboratorio —que **no se commiteó**— saltea la primera mitad a propósito y fuerza
el caso raro:

| build | n | `k` | `t` | `t − 240k` |
|---|---|---|---|---|
| normal | 382 | 1 (×381) | 513,65 – 513,91 | 273,65 – 273,91 |
| **caso A forzado** | **70** | **0 (×70)** | **273,66 – 273,71** | 273,66 – 273,71 |

**Los dos estados difieren en 240,00 exactas, y la latencia real es la misma en
los 452 informes de los dos firmwares: `t − 240k` = 273,7 ± 0,13.**

### El veredicto

**La carrera de §10 queda confirmada, y el pipeline exonerado.** Los 240 son el
origen de la ventana de captura, no tiempo de vuelo: se mueven medio buffer sin
que el audio se mueva un sample.

Con eso hay que releer un número de §10. Los **513,67** que ahí figuran como
"latencia absoluta del lazo" llevan **240 muestras de puro corrimiento de
ventana**. El número comparable entre corridas —y el que hay que citar— es
**`t − 240k` = 273,7 muestras = 5,70 ms**.

### Lo que NO se hizo, a propósito

**No se arregló.** Ahora que el informe imprime `k`, el artefacto se
autoidentifica y `t − 240k` sale bien en los dos estados: la medición ya no lo
necesita. El arreglo, si alguna vez se quiere, es armar la captura **antes** de
procesar las mitades del RX (arriba de `Service_Fills`, no en el fill del banco
24) — pero eso mueve el origen de la ventana otra vez y obliga a rehacer las
comparaciones de §8 y §11. No vale la pena por un modo de banco que ya sabe
decir en qué estado midió.
