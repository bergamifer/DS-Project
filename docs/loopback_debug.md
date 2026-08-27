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

> ⚠️ **LOS DOS NÚMEROS DE ESTA SECCIÓN ESTÁN MAL — CORREGIDOS EN §19.3.**
> Medido el 2026-08-26 con el lazo cerrado: la ganancia es **+29 dB** (no +49) y
> el pasaalto está en **~70 Hz** (no en 723 Hz). El factor 10 está en la
> resistencia efectiva de ganancia. Tres medidas independientes coinciden.
> **En 800 Hz la respuesta es plana**, así que la advertencia sobre el modo 17
> que sigue más abajo tampoco vale. Leer §19.3 antes de usar nada de acá.


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

> ⚠️ **ESTA SECCIÓN SE APOYA EN LOS +49 dB DE §2.1, QUE SON +29 dB.** Ver
> §19.3 y §19.4. Con la ganancia real **sobran ~20 dB de margen**: a −40 dB el
> amplificador entrega del orden de 600 mV, no 3,2 V. La conclusión de abajo
> —que hay que bajar el volumen— apunta en la dirección correcta pero con la
> magnitud equivocada. **El techo real quedó sin medir** (§19.5): hay que
> hacerlo con los parlantes desconectados.


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
   SPK+ ──[ 4,7k ]──┬──────────────── RJ45 pin (+)
                    │
                   [1k]     [2,2n]     ← en paralelo entre sí
                    │         │
   SPK− ──[ 4,7k ]──┴─────────┴─────── RJ45 pin (−)
```

> Construido con **4,7k / 2,2 nF** en vez de 10k / 10 nF (2026-08-26, decisión
> del usuario por lo que había en stock). Las dos desviaciones apuntan en
> sentidos opuestos y hay que leerlas juntas — ver el pasabajos abajo.
>
> ⚠️ Si en el futuro aparecen capacitores de 10 nF, **repetir la prueba** con
> ellos: la diferencia de fc no es cosmética (ver tabla), y sólo entonces el
> filtro queda tan lejos de la banda de prueba como se había calculado.

- **División**: 1k / (4,7k + 1k + 4,7k) = **1/10,4 = −20,3 dB**. Con el
  amplificador a fondo (3,2 V) entrega **307 mV** al ADC: bien arriba del piso
  de ruido (437 cuentas) y lejos de saturarlo. (Con 10k daba 150 mV a −26 dB;
  la versión con 4,7k llega con ~6 dB más de señal, margen extra para cuando
  haga falta bajar el volumen de trabajo.)
- **Pasabajos**: R equivalente = 1k ∥ 9,4k = 904 Ω. El capacitor si decide todo:

  | capacitor | fc | a 20 kHz (borde de la prueba) | a 250 kHz (portadora del amp) |
  |---|---|---|---|
  | 10 nF (cálculo original) | 17,6 kHz | −3,6 dB, corregible | ~−23 dB de rechazo |
  | **2,2 nF (lo que se usó)** | **~80 kHz** | **−0,26 dB, ~14° — casi nada** | **~−10 dB de rechazo** |
  | 100 nF (descartado) | 1,76 kHz | corta la banda de audio | sobra, pero inútil: mata lo que se quiere medir |

  Con 2,2 nF el filtro **casi no toca** la banda 20 Hz-20 kHz que es lo que se
  quiere medir sin corregir nada — pero deja **~13 dB menos** de rechazo a la
  portadora de 250 kHz que el diseño original. No es necesariamente un
  problema: el ferrite de salida del amplificador filtra antes de esto, y el
  filtro es **idéntico en los cuatro adaptadores**, así que si algo de
  portadora se cuela aparece igual en los cuatro y no contamina las
  mediciones *relativas* entre bancos (§5.1, §5.3). Sí podría subir el piso de
  ruido en una medición *absoluta*, algo a revisar si aparece.
- A 2 kHz el corrimiento de fase con 2,2 nF es despreciable (<2°), y de todos
  modos sería igual para todos los canales que se midan con el mismo
  adaptador, así que se cancela en cualquier medición relativa.
- Resistencias al 1 % y capacitores NP0/C0G si se busca precisión de nivel; para
  fase y retardo la tolerancia no importa mientras los adaptadores sean iguales.
- ⚠️ **Usar el mismo valor en los cuatro adaptadores** (no mezclar componentes
  distintos entre ellos) — es lo único que rompería la comparación relativa
  entre bancos.
- **Si se consiguen capacitores de 10 nF más adelante**: repetir §5.1/§5.5 con
  ellos. Con 2,2 nF el rechazo de portadora es menor; si algún resultado sale
  raro (piso de ruido más alto de lo esperado, algo con pinta de aliasing),
  éste es el primer sospechoso a descartar volviendo al valor de diseño.

⚠️ **Empezar siempre con el volumen abajo** y subir mirando `adc_pk` en
`/status`. El divisor protege del nivel nominal, no de un error de conexionado.

### 4.3 Dónde se enchufa cada punta (trazado del netlist, 2026-08-26)

La §4.2 dice qué construir; faltaba **a qué conector va**. Sale de recorrer
`DS.txt` de JSPK → FB → PAM8302 → VOUT del PCM1690, y de J9 → AK5538.

> ⚠️ **La fila del banco 16 no coincidió con la placa real — ver §19.0.** El
> cruce existe, pero **el número serigrafiado y la posición física no van
> juntos en ese par**: medido con el walker, ch16 sale por el conector que en
> la placa se lee **JSPK09**. Para conectarse, usar la posición y confirmar con
> el walker, no la etiqueta.

**Salida — qué banco es cada JSPK.** Cada conector lleva dos canales
consecutivos: pines **1/2 = el par** y **4/3 = el impar**.

| banco | DAC | canales | conectores | canal más bajo del banco |
|---|---|---|---|---|
| 0 | U2 (0x98) | 0-7 | JSPK01-04 | **ch0 → JSPK01 pines 1/2** |
| 8 | U3 (0x9C) | 8-15 | JSPK05-08 | **ch8 → JSPK05 pines 1/2** |
| 16 | U1 (0x9A) | 16-23 | JSPK09-12 ⚠️ | **ch16 → JSPK10 pines 1/2** |
| 24 | U4 (0x9E) | 24-31 | JSPK13-16 | **ch24 → JSPK13 pines 1/2** |

⚠️ El banco 16 es el del cruce: `VOUT1/VOUT2` (ch16-17) salen por **JSPK10** y
`VOUT3/VOUT4` (ch18-19) por **JSPK09** — el swap de `ds11_upgrades.md`, acá
confirmado desde el cobre. Los otros tres bancos van en orden.

> **Cae parado con J13**: J13 toma `U4 VOUT1` por R82, o sea **ch24**, que es
> exactamente el canal de JSPK13 pines 1/2. Un adaptador ahí y J13 puesto miden
> **el mismo canal antes y después del amplificador, en la misma captura** — que
> era lo que la §4 pedía para separar qué aporta cada etapa.

**Entrada — qué pin del RJ45 es qué canal del ADC.** Los cuatro jacks son un
solo cuerpo (J9); **1 y 2 están en paralelo**, y **3 y 4** también:

| jack | pines 1/2 | 3/4 | 5/6 | 7/8 |
|---|---|---|---|---|
| **1 y 2** | AIN1 = **c0** | AIN2 = **c1** | AIN3 = **c2** | AIN4 = **c3** |
| **3 y 4** | AIN5 = **c4** | AIN6 = **c5** | AIN7 = **c6** | AIN8 = **c7** |

En cada par el **pin impar es P (+)** y el **par es N (−)**.

Los cuatro adaptadores entran por el **jack 1** (c0-c3) y dejan libre el jack 3,
que es donde hoy vive J13 (c4). O sea: **una sola captura ve los cuatro bancos
amplificados más la toma limpia del ch24.** El firmware ya captura los 8
canales (`ADC_CHANNELS 8`), así que no hay que tocar nada para leerlos.

⚠️ El cable de entrada tiene que ser **a medida** (pines 4 y 6 intercambiados
respecto de T568B — ver `guia_operacion.md` §5): con un cable de red común, c1 y
c2 viajan por pares partidos. Para cuatro canales que se van a comparar en fase,
eso es justamente el ruido que no conviene meter.

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

---

## 14. RESULTADOS 2026-08-25 — respuesta en frecuencia por el lazo digital (Test D)

> ⚠️ **CORREGIDO EN §16.** Todo lo de abajo se midió con la placa **sin los
> 19-20 V**, y en ese estado la etapa analógica de salida del PCM1690 está
> hambreada. Dos conclusiones de esta sección **no sobreviven** a repetir la
> medición alimentada: el "pasabajo de 29,4 kHz" y los "1151 Ω en serie".
> **Leer §16 antes de usar cualquier número de acá.**

§5.5 pedía la respuesta en frecuencia de la cadena de salida. Acá está la parte
que se puede medir **sin 24 V y sin amplificadores**, que es lo que hoy está
disponible: J13 pincha la salida del DAC del canal 24, así que barriendo tonos
por el lazo digital sale amplitud contra frecuencia de la cadena **DAC → ADC**.

### El instrumento: modo 20, detección coherente

Modo de banco nuevo, `BARRIDO DE FRECUENCIA por el lazo`, emitiendo por los
**mismos** canales que el LOOPBACK (0/8/16/24, en fase) para que el día que
estén los adaptadores de §4.2 la misma corrida dé los cuatro bancos.

22 puntos log-espaciados de 20 Hz a 20 kHz, 100 ms de asentamiento y 200 ms de
medición cada uno: **~7 s por barrido**, y se repite.

**Detección coherente, no pico.** El pico mide señal más ruido, y en los
extremos —donde la respuesta cae— eso es un piso que miente: la curva se aplana
en el ruido en vez de seguir bajando. Multiplicando por seno y coseno de la
misma frecuencia y acumulando, el ruido se promedia a cero. Acá no es un lujo:
el tono llega a **~2600 cuentas** contra un piso de ruido de **~437**, o sea
15 dB — y aun así el barrido resuelve **0,03 dB**. Sin la ganancia de proceso de
las 9600 muestras, esta medición no existía.

La referencia sale de `sine_table_1khz`, la misma tabla e interpolación de los
modos de tono. Igual que con los retardos Q8 en §11: si se reimplementara, el
barrido mediría su propia copia.

### El resultado — 6 barridos, sin alimentación de 24 V

| f | medido | 1 polo | resid |
|---|---|---|---|
| 20 Hz | −2,18 dB | −2,16 | −0,02 |
| 30 Hz | −1,09 | −1,09 | 0,00 |
| 45 Hz | −0,51 | −0,52 | +0,01 |
| 63 Hz | −0,21 | −0,27 | +0,06 |
| 125 Hz | −0,04 | −0,07 | +0,03 |
| **723 Hz** | **+0,01** | −0,00 | +0,01 |
| **800 Hz** | **+0,01** | −0,00 | +0,01 |
| 1 kHz | 0,00 | −0,01 | +0,01 |
| 2,8 kHz | −0,03 | −0,04 | +0,01 |
| 8 kHz | −0,39 | −0,31 | −0,08 |
| 16 kHz | −1,13 | −1,12 | −0,01 |
| 20 kHz | −1,58 | −1,65 | +0,07 |

**Plana dentro de ±0,04 dB entre 125 Hz y 2,8 kHz.** Los dos extremos son un
solo polo cada uno, y ajustan muy bien:

| | frecuencia de corte | residual rms |
|---|---|---|
| pasaalto | **16,0 Hz** | 0,03 dB |
| pasabajo | **29,4 kHz** | 0,06 dB |

Repetibilidad entre barridos: **≤0,1 dB por encima de 45 Hz**, 0,2 dB en el
punto de 20 Hz. (El primer barrido después de un reset da ~0,2 dB de más en el
extremo grave: todavía se está asentando. Se descarta.)

### Lo que esto confirma, y no es la curva

§1 dedujo **de un archivo de PCB** que J13 pincha en R82 — o sea *después* de la
resistencia serie de 1 kΩ y *antes* del capacitor de acople de 220 nF — y se
advirtió expresamente: *"acá se está leyendo un archivo de diseño, no la placa
que tenés en la mano"*. **La respuesta en frecuencia lo verifica eléctricamente,
por las dos puntas:**

1. **Se ve el polo de 1 kΩ + 4,7 nF.** El pasabajos de 33,9 kHz que §2 derivó del
   esquemático aparece medido en 29,4 kHz. Si J13 pinchara directo en VOUT, antes
   de la resistencia, este polo no estaría.
2. **NO se ve el pasaalto de 723 Hz.** A 723 y a 800 Hz la respuesta es
   **+0,01 dB**. Si J13 pinchara después del acople de 220 nF, §2.1 predice
   −2,6 dB a 800 Hz. No hay nada.

Las dos cosas juntas ubican la toma exactamente donde §1 dijo, con la placa
enchufada en vez de con un archivo abierto.

### Dos resistencias que salen de la medición

Los polos, leídos al revés, dan valores que no estaban en ningún lado:

| polo | implica | contra el nominal |
|---|---|---|
| 29,4 kHz con 4,7 nF | **1151 Ω** en serie | 1 kΩ nominal → ~150 Ω de más |
| 16,0 Hz con 1,1 µF (los dos acoples de 2,2 µF en serie) | **9019 Ω** de lazo | menos 1 kΩ del DAC y 2×470 Ω → **~7,1 kΩ** de entrada del AK5538 |

Los ~150 Ω extra entran cómodos en la tolerancia del capacitor (±10 % solo ya
lleva el polo a 30,8 kHz) más la impedancia de salida del propio PCM1690, que no
figura en el datasheet que tenemos. No se elige entre las dos: es lo que hay.

### ⚠️ Lo que este barrido NO mide

**No contesta §5.5 del todo.** Lo que se midió es DAC → cable → ADC. Fuera del
alcance quedan, y siguen necesitando los adaptadores de §4.2 **y** alimentación:

- el pasaalto de **723 Hz** del amplificador, que es el número de §2.1 que más
  importa porque el **modo 17 trabaja a 800 Hz**, media octava adentro del borde;
- todo el PAM8302A: sus +49 dB, su recorte y su portadora de clase D;
- el filtro de ferrite de salida.

Que la cadena de conversión sea plana a 800 Hz igual sirve, y no es poco: **si
alguna vez se mide un desnivel a 800 Hz, no es del DAC ni del ADC.** Queda un
solo sospechoso.

### Un detalle de implementación que casi cuesta caro

La primera versión hizo HardFault a mitad del informe, con `BFAR` apuntando
0x40 bytes después del final del DTCM. La causa: **`UART_Print` cede a
`Service_Fills`**, así que mientras se imprime la tabla el emisor sigue
corriendo — y en ese momento el índice del barrido ya vale `FS_NUM_PTS`, o sea
una posición fuera de `fs_freqs[]`. Se arregló pausando **antes** de informar, no
después, más un clamp en el índice.

> Vale como regla general de esta base: **cualquier cosa que imprima por UART
> está cediendo el control al pipeline de audio en el medio.** El estado tiene
> que ser válido *antes* de la primera línea, no después de la última.

---

## 15. Cuatro medidas más sin alimentar la placa (2026-08-25)

Con los 19-20 V todavía sin conectar quedaban bloqueados la escucha del `-O2`, el
skew entre bancos y todo lo acústico. Esto es lo que **sí** se pudo cerrar con
ST-Link, HTTP y el cable de J13.

### 15.0 ⚠️ Antes que nada: el volumen guardado estaba en 0 dB

Apareció al verificar, y es lo primero que había que arreglar **antes** de
enchufar la alimentación, no después. Del log de arranque de la placa:

```
VOLUME: 0 dB al boot (ATDA=0xFF)
```

`PCM1690_Init` **sí** escribe −24 dB, pero después `Apply_Master_Volume()` lo
pisa con el valor persistido en EEPROM (`main.c`, "Restore saved master volume"),
y en esta placa ese valor era **0**. El default de `Config_LoadDefaults` ya es
−40 dB —la recomendación de §3 está implementada— pero **nunca se aplica si la
EEPROM tiene otra cosa**.

Contra la cuenta de §3: a −24 dB el PAM8302A ya está 12 dB adentro del recorte.
A 0 dB son **36 dB**. Se dejó en **−40 dB**, verificado contra un reinicio
(`VOLUME: -40 dB al boot (ATDA=0xAF)`).

> Lección: un default correcto en el código no es un default correcto en la
> placa. Lo que gobierna es la EEPROM, y sólo el log de arranque lo dice.

### 15.1 Linealidad de la atenuación del PCM1690 — §5.4, la mitad medible

Barriendo el volumen y midiendo el punto de 1 kHz por J13 con el detector
coherente del modo 20:

| pedido | cuentas | medido | error |
|---|---|---|---|
| 0 dB | 2579 | 0,00 | +0,00 |
| −12 | 644 | −12,05 | −0,05 |
| −24 | 161 | −24,09 | −0,09 |
| −27 | 115 | −27,02 | −0,02 |
| −30 | 82 | −29,95 | +0,05 |
| −33 | 58 | −32,96 | +0,04 |
| −36 | 41 | −35,97 | +0,03 |
| **−40** | 26 | **−39,93** | **+0,07** |
| −43 | 18 | −43,12 | −0,12 |
| −46 | 13 | −45,95 | +0,05 |

**La atenuación digital es exacta dentro de ±0,13 dB de 0 a −46 dB**, sumando
también un barrido en pasos de 3 dB de 0 a −24. El control de volumen hace
exactamente lo que dice, y el punto de operación recomendado (−40 dB) cae dentro
del rango verificado.

⚠️ **No contesta §5.4 entera.** Dónde deja de ser lineal *el amplificador* —el
volumen máximo utilizable del instrumento— sigue necesitando alimentación: J13
mide antes del PAM8302A.

> El primer barrido se cortó en −24 dB, y no por ruido: el umbral de "entrada
> viva" del propio modo estaba en 100 cuentas, o sea 24 dB por encima del piso
> real de 1-3 cuentas. **El instrumento se estaba censurando solo justo en el
> rango que importaba.** Bajado a 10.

### 15.2 Retardo de grupo contra frecuencia

§8 y §9 **asumen** que los group delays del PCM1690 y del AK5538 son constantes
de datasheet, y toda la conclusión de §9 se apoya en esa premisa. Ahora está
medida: el modo 20 mide cada punto dos veces, a f y a f+40 Hz, y saca el retardo
de la diferencia de fase.

| | |
|---|---|
| retardo en banda media (723 Hz – 8 kHz) | **513,76 muestras** |
| dispersión sobre 6 barridos | **≤0,15 muestras** |

**Dos cruces que valen más que el número:**

**1. Contra el loopback de §13.** La correlación de chirp daba **513,67**. Dos
instrumentos independientes —correlación en el tiempo contra pendiente de fase en
frecuencia— coinciden en **0,09 muestras = 1,9 µs**.

**2. Contra la amplitud de §14, sin ajustar nada.** El exceso de retardo a baja
frecuencia tiene que ser el del pasaalto de 16,0 Hz que §14 sacó de la *amplitud*.
Prediciendo con ese polo, promediado sobre el mismo par f..f+40:

| f | exceso medido | predicho por el polo de 16 Hz | residuo |
|---|---|---|---|
| 30 Hz | +53,68 | +50,66 | +3,02 |
| 45 | +31,45 | +29,71 | +1,74 |
| 90 | +10,77 | +10,21 | +0,55 |
| 250 | +1,78 | +1,68 | +0,10 |
| 500 | +0,41 | +0,45 | −0,04 |
| 1000 | +0,03 | +0,12 | −0,08 |

De 250 Hz para arriba el residuo es **≤0,14 muestras**. Amplitud y fase describen
el mismo polo, medidos por caminos distintos.

**Arriba, en cambio, no cierra — y eso es el hallazgo.** A 16 kHz el exceso es
+0,96 muestras y a 20 kHz **+2,42**, cuando el polo de 29,4 kHz predice ~0
respecto de la banda media (su contribución es casi plana). **Ese exceso no es la
red RC: son los filtros digitales de los conversores.** Son 2,42 muestras =
50 µs = 17 mm de camino acústico a 20 kHz.

**Lo que importa para §9:** en la banda donde el instrumento trabaja de verdad
(500 Hz – 2,8 kHz) el retardo es constante dentro de **±0,2 muestras = 4 µs =
1,4 mm**. Tratar los group delays como constantes está **justificado, y ahora
medido**.

### 15.3 Diafonía entre entradas del ADC

> ⚠️ **SUPERADO POR §16.3.** Medido sin alimentar, con 65 veces menos excitación:
> lo de abajo es el piso de ruido, no diafonía. Los números reales están en §16.3.

Con el tono entrando sólo por entrada5, las otras siete:

| entrada | peor caso | a qué frecuencia | cuentas |
|---|---|---|---|
| c0 | −56,9 dB | 20 Hz | 3 |
| c1 | −62,5 | 125 Hz | 2 |
| c2 | −66,2 | 30 Hz | 1 |
| c3 | −68,0 | 45 Hz | 1 |
| c5 | −63,2 | 30 Hz | 2 |
| c6 | −63,1 | 20 Hz | 1 |
| c7 | −57,9 | 20 Hz | 3 |

Todas en 1-3 cuentas, y el peor caso cae **siempre en 20-125 Hz**. Eso es la
firma del piso de ruido (1/f), no de acoplamiento: si fuera diafonía real
seguiría a la señal, no al ruido. **La diafonía del ADC queda acotada a
<−57 dB**, que es el piso de la medición, no un valor medido.

⚠️ **No reproduce ni refuta los −38 dB del 2026-08-18.** Aquélla era otra
configuración —la Mac excitando c6/c7, con el par partido del pinout de J9 y el
retorno de masa común como sospechosos— y hace falta esa fuente para repetirla.
Lo que sí hace es **acotar al conversor**: con esta excitación el ADC no acopla
ni cerca de −38 dB, así que el sospechoso queda del lado del cableado.

### 15.4 La fase RX↔TX, por HTTP — nivel 2 de multinode

§9 dejó el hallazgo práctico: la fase sale de **dos registros**, sin loopback,
sin J13, sin adaptador y sin micrófono. Estaba sólo en el heartbeat, o sea que
leerla pedía un cable serie. Ahora viaja en `/status` (protocolo **v10**,
85 bytes, campo `dma_phase`):

```json
{"proto_ver":10, "dma_phase":0, "status_len":85, ...}
```

Cualquier nodo de una instalación de 10-16 cajas puede decir por red en qué
estado de fase arrancó. **Eso era todo lo que faltaba para el nivel 2.**

> Detalle que apareció midiendo: la primera versión informaba la fase en
> `[0, 480)` y leía **`0` o `479`** alternando. Son los ±1 de ruido de lectura de
> §9 — pero `479` se lee como **medio buffer de diferencia** en vez de una
> muestra. El campo se pasó a **con signo, envuelto alrededor de cero**, que es
> la forma en que se va a comparar entre nodos.

### Un instrumento que casi miente: la fase del emisor

Para que las dos sub-medidas de cada punto compartan origen de fase, la primera
versión ponía `fs_tx_phase = 0` al abrir cada punto. **El retardo salió perfecto
y la amplitud se rompió**, de forma perfectamente repetible en 7 barridos:

| f | con reset | sin reset | |
|---|---|---|---|
| 1000 Hz | 2358 | 2617 | **−10 %** |
| 2000 Hz | 2105 | 2607 | **−19 %** |
| 4000 Hz | 1704 | 2583 | **−34 %** |
| 20000 Hz | 1465 | 2136 | **−31 %** |
| el resto de la tabla | — | — | ±2 % |

Un A/B con y sin esa única línea lo dejó sin ambigüedad: con reset, retardo bueno
y amplitud rota; sin reset, amplitud buena y retardo basura.

**La solución fue anotar la fase del emisor en vez de pisarla.** El mismo origen
común se consigue guardando `fs_tx_phase` al abrir el punto y sumándolo a la fase
detectada al informar — **sin cambiar un solo sample emitido**. Con eso salen las
dos cosas en el mismo barrido, que es lo que está commiteado.

> ⚠️ **Pregunta abierta, a propósito.** *Por qué* pisar la fase degradaba la
> amplitud, y sólo en frecuencias cuyo incremento de tabla es una potencia de dos
> (250, 500, 1000, 2000, 4000 Hz) pero no en 8000 ni 16000, **no quedó
> identificado**. No bloquea nada —el camino que se usa no toca la emisión— pero
> queda escrito con los datos del A/B en vez de enterrado. Si alguna vez se toca
> el generador de tonos, esto es un aviso: **el modo 20 puede medir una amplitud
> equivocada sin ninguna bandera de error.**

---

## 16. RESULTADOS 2026-08-26 — lo mismo, pero con la placa alimentada

Al conectar los 19-20 V y repetir el modo 20 **sin tocar nada más**, la medición
cambió tanto que obligó a revisar §14. Base: **145 barridos completos**, volumen
−40 dB verificado en el registro de los cuatro DAC, amplificadores deshabilitados.

### 16.0 Por qué cambió: el PCM1690 estaba corriendo con la salida hambreada

| | sin 19-20 V (§14) | con 19-20 V |
|---|---|---|
| nivel en 1 kHz | 2.600 cuentas @ **0 dB** | 168.792 @ **−40 dB** |
| normalizado a igual volumen | — | **+76 dB** |
| polo agudo | −1,7 dB @ 20 kHz | **−0,17 dB @ 20 kHz** |
| pasaalto | 16,0 Hz | 17,8 Hz |
| retardo de grupo en 1 kHz | 513,76 smp | **513,70 smp** |

`presupuesto_potencia.md` §1: *"el único consumidor [de la tensión de entrada] es
el buck (de ahí salen +5VA para los amps, y por FB2+D9 el +5V que alimenta los
reguladores de 3,3 V)"*. Sin los 19-20 V la placa se alimentaba por USB a través
de D9: **lo digital funcionaba y la etapa analógica de salida no tenía su riel.**

Las tres firmas encajan y no dejan otra lectura razonable:

- **El pipeline digital es idéntico** — el retardo de grupo no se movió ni una
  décima de muestra (513,70 vs 513,76). Nada cambió en el camino de datos.
- **El lado del ADC es idéntico** — el pasaalto sigue donde estaba.
- **Sólo cambió la salida del DAC** — 76 dB más de nivel y el polo agudo se fue
  de la banda, que es exactamente lo que hace una etapa de salida que pasa de
  estar starved (poco swing, impedancia alta) a tener su riel.

> **Lección que vale más que el número:** durante todo el 2026-08-25 el
> instrumento dio resultados **repetibles a 0,03 dB, coherentes entre sí, y con
> un modelo de dos polos que ajustaba a 0,06 dB de residuo**. Nada gritaba
> "error". Y sin embargo describía un circuito que no era el que está en la
> placa. **La repetibilidad no es exactitud**, y un ajuste bueno sobre datos
> tomados en un estado equivocado da confianza falsa, no verdad.

### 16.1 Lo que hay que corregir de §14

| afirmación de §14 | estado |
|---|---|
| "pasabajo de 29,4 kHz — se ve el polo de 1 kΩ + 4,7 nF" | ❌ **Falso.** Artefacto del riel hambreado. Alimentada no hay ningún polo en banda |
| "1151 Ω en serie, ~150 Ω más que el nominal" | ❌ **Sin sentido.** Derivado del polo anterior |
| "J13 pincha después de R82" (mitad basada en el polo agudo) | ⚠️ **Ese argumento se cae** |
| "J13 pincha después de R82" (mitad basada en la ausencia del pasaalto de 723 Hz) | ✅ **En pie, y ahora vale más**: hoy el riel del amplificador SÍ está presente y 723/800 Hz siguen leyendo +0,01 dB |
| "pasaalto de 16,0 Hz" | ✅ Confirmado, ahora en **17,8 Hz** con residuo 0,020 dB |
| "~7,1 kΩ de entrada del AK5538" | ✅ En pie (sale del pasaalto, que no cambió) |
| "plano ±0,04 dB de 125 Hz a 2,8 kHz" | ✅ Superado: **±0,05 dB de 250 Hz a 8 kHz** |

### 16.2 La respuesta real DAC→ADC

| | |
|---|---|
| pasaalto | **17,8 Hz**, residuo RMS **0,020 dB** |
| plano de 250 Hz a 8 kHz | **±0,05 dB** |
| en 20 kHz | **−0,17 dB** |
| repetibilidad entre 145 barridos | **0,005 a 0,023 dB** |

**No hay polo agudo dentro de la banda de audio.** El pasabajos de 33,9 kHz que
§2 derivó del esquemático queda demasiado arriba para verse acá — o sea que §2
sigue siendo correcto, pero **§14 lo "confirmó" por casualidad, midiendo otra
cosa**.

El retardo de grupo se repite clavado (§15.2 vale entero): 513,70 muestras en
1 kHz, +2,48 en 20 kHz. **Que no se moviera con un cambio de 76 dB en el nivel es
la mejor prueba de que mide el pipeline y no el analógico.**

> ⚠️ El `tau` de 20 Hz es basura (dispersión de 1080 muestras entre barridos):
> a esa frecuencia el par de 40 Hz da una diferencia de fase que envuelve. **De
> 30 Hz para arriba es sólido.** Anotado como límite del método, no como falla.

### 16.3 Diafonía de verdad — reemplaza §15.3

Con 65 veces más excitación, esto ya no es piso de ruido. Peor caso de 145
barridos, con el tono en entrada5 (c4):

| entrada | peor caso | dónde | cuentas | en 1 kHz |
|---|---|---|---|---|
| c0 | −90,0 dB | 20 Hz | 4 | −113,9 dB |
| c1 | −90,4 dB | 20 Hz | 4 | −115,1 dB |
| c2 | −92,0 dB | 20 Hz | 3 | −114,9 dB |
| c3 | −90,7 dB | 20 Hz | 4 | −113,2 dB |
| **c5** | **−73,5 dB** | **20 kHz** | 35 | −99,4 dB |
| **c6** | **−82,9 dB** | **20 kHz** | 12 | −107,9 dB |
| c7 | −90,2 dB | 20 Hz | 4 | −114,9 dB |

**Dos resultados distintos en la misma tabla:**

- **c0-c3 y c7 están en el piso** (3-4 cuentas, peor caso en 20 Hz, o sea 1/f).
  Su diafonía es **<−90 dB**: no medible, y no hace falta.
- **c5 y c6 son acoplamiento real.** Crecen con la frecuencia —firma capacitiva—
  y son **justo los dos vecinos del canal excitado**. −73,5 dB y −82,9 dB en
  20 kHz, contra −99,4 y −107,9 en 1 kHz.

En la banda de trabajo del equipo la diafonía es **mejor que −99 dB en todos los
canales**, o sea irrelevante para el beamforming. El acoplamiento entre vecinos
existe pero vive arriba de 10 kHz.

⚠️ Sigue **sin reproducir ni refutar los −38 dB del 2026-08-18**: aquello era la
Mac excitando c6/c7 por el par partido de J9. Lo que §16.3 agrega es que **el
conversor y el ruteo interno están 35 dB mejor que eso**, así que el sospechoso
queda firmemente del lado del cable y el conector.

---

## 17. El error de fase entre bancos: real, disparaba, y no contaminaba (2026-08-26)

Antes de creerle a §5.1 (skew entre bancos) había que descartar que **el propio
modo introdujera** el desfasaje que quiere medir. Resultado: **el error existe y
dispara, pero nunca durante una medición.** §5.1 se puede correr con confianza.

### 17.1 Qué estaba mal

`Service_Fills` llena las dos mitades de cada banco en **bloques `if`
separados**, y recorre los cuatro bancos en orden 0→8→16→24. Los emisores del
modo 19 (chirp) y del 20 (tono) leían una posición **compartida** y sólo la
avanzaban en el banco 24 — a propósito, para que dentro de una pasada los cuatro
emitieran el mismo trozo.

Eso vale mientras cada pasada llene **una** mitad por banco. Si el lazo principal
se demora más de 5 ms, la pasada llena **las dos**:

| | primera mitad | segunda mitad |
|---|---|---|
| bancos 0, 8, 16 | trozo P | **trozo P otra vez** |
| banco 24 | trozo P | **trozo P+240** |

**240 muestras (5 ms) de desfasaje entre el banco 24 y los otros tres** — contra
los microsegundos que §5.1 quiere resolver. Silencioso: ninguna bandera de error.

### 17.2 Cómo se comprobó

Contador `dbl_fill[4]`, que cuenta las pasadas en que un banco llenó sus dos
mitades, publicado en el heartbeat:

```
dblfill: b0=7 b8=7 b16=7 b24=7
```

| prueba | resultado |
|---|---|
| Arranque | **7 pasadas dobles**, iguales en los cuatro bancos |
| 12 reinicios seguidos | siempre 7, y siempre **ya alcanzado antes de la primera captura** |
| 63 informes de loopback | **no incrementa** |
| 145 barridos del modo 20 | **no incrementa** |
| Guardado de EEPROM real (~12 ms) en plena medición | **no incrementa**, captura limpia |
| Primeras capturas de los 12 arranques | `t` entre 513,58 y 513,60, `q` entre 1526 y 1536 — sin una sola anomalía |

**Por qué no dispara en operación:** todos los caminos bloqueantes conocidos
ceden. `UART_Print` llama a `Service_Fills` entre carácter y carácter (el arreglo
del 2026-08-14 contra los "pops"), y `EEPROM_Yield`, `UART_Protocol_Yield` y
`OTA_Yield` hacen lo mismo. Las 7 pasadas dobles caen en la inicialización,
cuando esas cesiones todavía no están en juego.

> Vale registrar que **el arreglo de los "pops" del 2026-08-14 estaba
> protegiendo, sin saberlo, la validez de las mediciones de skew.** Si alguien
> alguna vez "optimiza" `UART_Print` volviéndolo bloqueante, no va a romper el
> sonido: va a romper el instrumento, en silencio.

### 17.3 El arreglo

Cada banco lleva **su propia posición** (`lb_bank_pos[4]`, `fs_bank_ph[4]`) y la
avanza cuando llena una mitad. El resultado deja de depender de cómo
`Service_Fills` agrupe las mitades. El estado (armado, fin de chirp) sigue
gobernado por el banco 24, que es como se calibró §13.

Verificado idéntico en el caso normal: `t=513,58`, `q≈1533`, antes y después.

> **Por qué arreglarlo si no disparaba.** Porque la protección era accidental:
> depende de que *todo* camino bloqueante futuro se acuerde de ceder. Un
> `HAL_Delay` nuevo en cualquier lado reintroduce 5 ms de skew en el modo que
> mide skew, sin ninguna señal. El costo del arreglo fue un arreglo de cuatro
> posiciones.


---

## 18. RESULTADOS 2026-08-26 (tarde) — el adaptador no entrega nada, y lo que sí salió

Con los cuatro adaptadores armados (4,7k / 1k / 2,2 nF, §4.2) y conectados por
primera vez. **La medición de skew entre bancos NO se pudo hacer**: por los
adaptadores no llega señal. Lo que sigue es la evidencia de que el problema está
del lado del cableado y no de la placa, más dos resultados que sí salieron de
la sesión.

### 18.1 🔴 Las cuatro entradas del adaptador están mudas

| | |
|---|---|
| **Esperado en c0-c3** a −40 dB | del orden de **10⁶ cuentas** (§4.2: el amp cerca de su techo entrega 307 mV al ADC) |
| **Medido** | **11 a 18 cuentas** — el piso de ruido |
| Faltante | ~**100 dB** |

No es "poca señal": es **circuito abierto**. Las pruebas que lo acotan:

- **Con detección coherente**, que tiene ~40 dB de ganancia de proceso y ve muy
  por debajo del ruido: c0=1, c1=2, c2=120, c3=210 cuentas contra
  **4.266.938** del canal de J13. Si hubiera algo conectado y muy atenuado,
  aparecería. No aparece.
- **Barriendo el volumen 28 dB** (−40 → −12): c4 sube ×25,4 exacto y c0-c3 **no
  se mueven**. Una conexión mala pero existente escalaría; el ruido no.
- **Con los amplificadores encendidos**, verificado por `/status` (`amps:true`),
  no por suposición.
- `adc_ovf = 0`: no es que sature y se lea raro.

**Lo que queda descartado** (o sea, la placa está bien):

- **El DAC anda**: J13 (ch24) escala perfecto sobre 28 dB — ver §18.2.
- **Las entradas c0-c3 convierten**: muestran diafonía desde c4 con el mismo
  patrón que c5/c6, que son entradas sanas. No están muertas.
- **El emisor llega a los cuatro bancos**: el chirp se escribe directo al buffer
  de TX (`buffer[i*AUDIO_CHANNELS + ch]`), sin pasar por mutes ni ganancias, y
  `SYNC bancos` sigue informando los cuatro.

**Lo que falta comprobar, y sólo se puede con la placa en la mano** — en orden,
porque el primero parte el problema al medio:

1. **Escuchar.** Modo 19, amps ON, volumen −40: cuatro parlantes (los de
   JSPK01/05/10/13) tienen que chirpear cada 2 s. Si suenan, el amplificador
   anda y el problema es **100 % del adaptador o del cable**. Si hay silencio,
   el problema es el amplificador o su alimentación.
2. **El adaptador con el téster, desenchufado de todo.** Tres números:

   | entre | tiene que dar |
   |---|---|
   | los dos pines del JST | **10,4 kΩ** (4,7k + 1k + 4,7k) |
   | los dos pines del RJ45 | **1 kΩ** (la resistencia de shunt) |
   | pin 1 del JST ↔ pin (+) del RJ45 | **4,7 kΩ** |

   Si el primero da infinito, falta una soldadura. Si el segundo da infinito,
   el shunt está abierto y no habría divisor. Si da ~0, hay un puente.
3. **+5VA presente.** Es el riel de los 32 PAM8302A y **no tiene test point**:
   se mide en el pad **+ de C290** (`ds11_errata.md` falla #2 — C290 está
   removido, así que el pad está accesible). Tiene que haber ~5 V con los
   19-20 V puestos. Si no hay, los amps no tienen de dónde sacar la señal y el
   síntoma sería exactamente éste: DAC perfecto, amplificadores mudos.

> **Por qué el orden importa**: la prueba 1 cuesta cinco segundos y decide si
> hay que mirar el adaptador (2) o la placa (3). Hacer las tres a ciegas es
> trabajo de más.

#### Segunda vuelta, con los datos del usuario (misma tarde)

Fernando aclaró: está en **JSPK01/05/09/13**, el RJ45 en los pines J9_1-J9_8
(o sea c0-c3), J13 sigue puesto, y **los parlantes NO están conectados** — así
que la prueba de escuchar no se podía hacer. Preguntó, con razón, si los
amplificadores estaban encendidos durante las mediciones.

**Sí lo estaban, y no es una suposición.** `amps:true` en `/status` sale de
`state_flags` bit 0, que es `amps_enabled`, y esa variable se escribe **sólo**
dentro de `Set_Amps()` — en la línea siguiente a la que pone PD3 en alto. Si
`/status` dice true, PD3 se manejó.

> **Por qué parecía que no**: el `AMPS:` del serie nunca aparecía en el log. La
> causa no era el firmware sino **dos lectores peleándose el puerto**: había una
> sesión de `screen` del usuario tomando `/dev/cu.usbmodem21102` al mismo tiempo
> que el logger, y cada uno se robaba bytes del otro. Se ve en el log en líneas
> partidas como `Half: 100/, Err: 0, RX 47213 byte`. **Lección: con el serie
> compartido, la ausencia de una línea no prueba nada; `/status` sí.**

Dos correcciones y una prueba que cierra el tema:

1. **JSPK09 no es el banco 16 — es JSPK10.** Es el cruce de §4.3: `VOUT1/VOUT2`
   (ch16-17) salen por **JSPK10** y `VOUT3/VOUT4` (ch18-19) por JSPK09. Con el
   adaptador en JSPK09 ese canal escucha **ch18**, y el chirp del modo 19 sale
   por ch16. Explica **una** de las cuatro entradas mudas, no las cuatro.
2. **Se repitió todo con el modo 12** (`TX 2k ondulante centro @1m`), que emite
   un tono de 2 kHz **por los 32 canales a la vez**. Eso borra por completo la
   variable de "en qué JSPK está cada adaptador": emitan lo que emitan, todos
   los conectores tienen señal.

**Resultado: las OCHO entradas del ADC siguen en el piso de ruido**, con los
amps encendidos y barriendo el volumen de −55 a −30 dB:

| | c0 | c1 | c2 | c3 | c4 (J13) | c5 | c6 | c7 |
|---|---|---|---|---|---|---|---|---|
| cuentas | 346 | 435 | 108 | 224 | **620.959** | 98 | 98 | 92 |

Se revisaron también c5-c7 —no sólo c0-c3— para descartar que el RJ45 estuviera
en el jack 3 o 4 en vez del 1. No lo está: **no llega señal de parlante a
ninguna entrada del ADC.**

#### Qué queda, y cómo se decide con un téster

Descartados con evidencia: el DAC, las ocho entradas del ADC, el emisor, PD3, y
el mapeo de conectores. Quedan dos, y **no se distinguen por red**:

- **Los amplificadores no están sacando señal** — y el sospechoso con historia es
  el riel **+5VA**: `ds11_errata.md` falla #2 documenta un corto de +5VA a GND
  reparado **con bisturí** tras remover C290. Si esa reparación abrió la pista, o
  se abrió después, el síntoma es exactamente éste.
- **El adaptador o el cable.**

> ⚠️ **La placa no puede avisar de esto sola.** +5VA no tiene test point ni
> telemetría: nada en el sistema lo sensa. El firmware informa `amps:true` con
> toda honestidad —PD3 está alto— aunque el riel esté muerto. **Candidato fuerte
> para la DS12: un divisor de +5VA a un ADC del STM32.** Convierte media tarde de
> diagnóstico en un campo de `/status`.

**Prueba A — la raíz, con cualquier téster (30 s).** DC en el pad **+ de C290**
(está removido, el pad queda accesible): tiene que haber **~5 V**. Si da 0, ahí
terminó el diagnóstico.

**Prueba B — el límite entre placa y adaptador.** Con el modo 12 corriendo y los
amps encendidos, **AC volts directo en JSPK01, pines 1 y 2** (sin el adaptador
en el medio): tiene que haber **volts, no milivolts** — a −40 dB el amplificador
está recortando contra su riel de 5 V, así que son unos 3 V<sub>rms</sub>. Un
téster barato pierde precisión en 2 kHz pero para "hay o no hay" alcanza.

- Hay volts en JSPK01 y nada en el ADC → **es el adaptador o el cable** (seguir
  con los tres números de la tabla de arriba).
- No hay volts en JSPK01 → **es la placa**, y la prueba A dice si es +5VA.

⚠️ **Cuando el adaptador funcione, ojo con el nivel.** Con +49 dB de ganancia
del PAM8302A (§2.1) y el divisor de −20,3 dB, a −40 dB de volumen la entrada
queda **cerca de saturar**, no cómoda. Subir desde MÁS abajo —−50, −55— mirando
`adc_pk`, y no dar por buena la recomendación de "−20 dB" de §4.3, que se
calculó suponiendo que la señal no llegaba.

### 18.2 La atenuación es lineal también arriba, y la cadena no depende del nivel

Salió gratis: el barrido de diagnóstico se corrió a −12 dB y a −40 dB, 3 y 4
barridos completos por nivel. **Es la primera vez que se mide el mismo camino a
dos niveles muy distintos.**

| | |
|---|---|
| referencia a −12 dB | 4.266.938 cuentas |
| referencia a −40 dB | 167.898 cuentas |
| razón medida | **28,10 dB** contra 28,00 comandados → **error +0,10 dB** |

§15.1 ya había medido la linealidad del atenuador (±0,13 dB de 0 a −46 dB) pero
**siempre sobre el mismo nivel absoluto de entrada**. Esto la confirma con la
entrada 28 dB más caliente, a −5,9 dBFS, o sea cerca del techo del ADC.

Y lo que más vale, comparando las dos curvas punto por punto:

- **De 63 Hz a 20 kHz las dos respuestas son idénticas dentro de 0,1 dB.**
- **De 250 Hz para arriba el retardo de grupo coincide dentro de 0,01 muestras.**

O sea: **la cadena DAC→ADC no depende del nivel**. Ni compresión, ni distorsión
que aparezca al subir, ni corrimiento de fase con la amplitud. Eso estaba
*supuesto* en toda medición relativa hecha por el lazo —cambiar el volumen no
cambia la función de transferencia— y hasta hoy no se había verificado.

> El único punto que no coincide es 20 Hz (tau +590 contra −599). Es el
> envolvimiento de fase del par de sub-medidas separadas 40 Hz, no una
> discrepancia: a 20 Hz el retardo de grupo por ese método es ambiguo. Los
> 21 puntos restantes coinciden.

### 18.3 Un instrumento que mentía con la excitación alta — arreglado

El barrido a −12 dB informó **+149 dB de diafonía** para c4. Un número imposible
que delató un error real en `FS_Report`.

**La causa**: la referencia de la diafonía era *la primera entrada que pasa el
umbral*, por índice. Con `FS_LIVE_MIN` en 100 nunca falló —ninguna entrada muda
llegaba a 100 cuentas—. Al bajarlo a **10** en §15.1 (para que el barrido de
volumen no se censurara solo en −24 dB) se abrió el agujero: con excitación alta
**la propia diafonía alcanza para que una entrada sin cable cruce las 10
cuentas**, y si su índice es menor que el del canal excitado, se queda con la
referencia. A −12 dB, **c2 con 11 cuentas de ruido le ganó la referencia a c4
con 4,27 millones**, y todo salió normalizado contra el ruido.

**El arreglo**: la referencia pasa a ser la entrada **más fuerte**, no la
primera. No tiene ese modo de falla —la excitación está decenas de dB sobre
cualquier acoplamiento— y el umbral sigue decidiendo qué entradas se imprimen,
que es para lo que se bajó.

> Es una regresión introducida al arreglar otra cosa, y no la habría encontrado
> ninguna medición a volumen de trabajo: sólo aparece con la excitación alta.
> Lo que la delató fue un número **absurdo** (+149 dB), no uno sutilmente
> equivocado — por eso conviene que los informes muestren la unidad física y no
> sólo un índice: un +149 dB salta, un "0,93" no.

### 18.4 La mejor diafonía medida hasta ahora

Con la referencia arreglada y 28 dB más de excitación que §16.3, varias entradas
salen del piso de ruido y pasan a ser **medidas** de verdad:

| entrada | §16.3 (−40 dB) | ahora (−12 dB) | qué significa |
|---|---|---|---|
| c0 | −80,6 (15 cta) | **−103,3** (1 cta) | seguía siendo piso; el real está aún más abajo |
| c1 | −74,5 (32 cta) | **−96,6** (2 cta) | ídem |
| c2 | −84,1 (10 cta) | **−90,8** (120 cta) | ahora sí medido |
| c3 | −79,1 (18 cta) | **−86,0** (210 cta) | ahora sí medido |
| c5 | −73,7 (34 cta) | **−73,7** (863 cta) | **idéntico** → acoplamiento real |
| c6 | −83,5 (11 cta) | **−83,8** (271 cta) | **idéntico** → real |
| c7 | −97,9 (2 cta) | **−97,5** (56 cta) | **idéntico** → real |

La prueba de que c5/c6/c7 son acoplamiento y no ruido es que **repiten el mismo
dB con 28 dB más de excitación** (0,3 dB de diferencia). Un piso de ruido no
hace eso: se queda donde está y el número relativo baja, que es exactamente lo
que hicieron c0 y c1.

Confirma §16.3: **c5 es el peor caso a −73,7 dB en 20 kHz**, es vecina del canal
excitado y sube con la frecuencia — firma capacitiva. En la banda de trabajo
sigue por debajo de −99 dB.

### 18.5 Firmware

La placa quedó corriendo **HEAD (`2CA685DB`)** por OTA. Antes informaba
`897BDF5C`: el arreglo de §17 se había flasheado desde el árbol de trabajo
*antes* de commitearlo, así que el `fw_id` era el del commit anterior y no había
forma de saber desde afuera qué estaba corriendo. Ahora coincide.

---

## 19. RESULTADOS 2026-08-26 (noche) — el lazo cerrado, por fin

Adaptadores resoldados y funcionando. **Se hizo la medición de §5.1**, la que más
falta hacía. Y de paso se cayeron dos números del análisis de esquemático de
§2.1 que venían gobernando cómo se opera el equipo.

Base: **84 informes completos** del modo 19 con las cinco entradas, volumen
−35 dB, más barridos del modo 20 a seis niveles.

### 19.0 El mapeo, medido en vez de supuesto

Con el **walker** (un canal por segundo) y la excitación de los modos 19/20 se
determinó qué canal excita cada entrada. **No coincide con lo que yo había
deducido del netlist para el banco 16, y el usuario tenía razón:**

| entrada | canal | banco | conector (según el usuario) |
|---|---|---|---|
| c0 | ch0 | 0 | JSPK01 |
| c1 | ch8 | 8 | JSPK05 |
| c2 | **ch16** | 16 | **JSPK09** |
| c3 | ch24 | 24 | JSPK13 |
| c4 | ch24 | — | J13 (toma directa del DAC) |

§4.3 decía que ch16 sale por **JSPK10**. La medición dice que sale por el
conector que el usuario llama **JSPK09**. La conclusión no es que el trazado del
cobre estuviera mal —el cruce existe— sino que **el número serigrafiado y la
posición física no van juntos en ese par**, que es exactamente lo que
`ds11_upgrades.md` avisaba al decir que el orden de los JSPK es parte del diseño
del arnés. **Para conectarse hay que usar la posición, no la etiqueta.**

> Lección de método: §4.3 salió de leer un archivo de diseño y se escribió con
> confianza. Alcanzó un walker de 32 segundos para corregirla. Cuando existe la
> posibilidad de medir, medir gana.

### 19.1 🔴 EL SKEW ENTRE BANCOS — la medición de §5.1

Retardo absoluto del lazo, por banco, en el **terminal de parlante** (o sea
incluyendo amplificador y adaptador), 84 informes:

| | t medio (muestras) | sigma |
|---|---|---|
| banco 0 | 513,888 | 0,034 |
| banco 8 | 514,179 | 0,031 |
| banco 16 | 513,961 | 0,030 |
| banco 24 | 513,967 | 0,009 |
| J13 (ch24, antes del amp) | 513,580 | **0,000** |

**Los tres bancos con adaptadores iguales** (0, 16, 24 — ver §19.2 por qué el 8
queda afuera):

| par | skew | |
|---|---|---|
| **banco 24 − banco 16** | **+0,12 µs** | los dos en SAI2 (master + esclavo) |
| banco 16 − banco 0 | +1,52 µs | SAI2 contra SAI1 |
| banco 24 − banco 0 | +1,63 µs | SAI2 contra SAI1 |

**Dos resultados, y los dos importan:**

1. **El sincronismo master↔esclavo dentro de un SAI es de 0,12 µs.** SAI2_A
   (banco 16) y SAI2_B (banco 24) salen a la misma muestra, medido del lado
   analógico. La arquitectura de arrancar el esclavo antes que el master
   (descubrimiento 5 de la memoria) queda **verificada eléctricamente**, no por
   sus consecuencias.
2. **Entre los dos bloques SAI hay ~1,5 µs.** Es la primera medición del lado
   que importa: hasta hoy los "4 µs" salían de comparar marcas de `DWT->CYCCNT`
   en los callbacks de DMA, o sea de cuándo se entera el software. El
   `SYNC bancos` que informa el heartbeat venía diciendo **1-2 µs** en esta
   sesión — **y coincide**. El invariante queda confirmado.

Para dimensionarlo: 1,5 µs son **0,5 mm de camino acústico**, contra los 80 mm
de separación entre parlantes. **No afecta el beamforming.** El equipo está
mejor sincronizado de lo que su propia documentación suponía.

### 19.2 ⚠️ El adaptador del banco 8 está mal armado — y dos síntomas lo prueban

El banco 8 daba **+6,05 µs** contra el banco 0, cincuenta veces el skew de los
otros. No es el hardware: **es su adaptador**, y hay dos medidas independientes
que lo dicen.

| | banco 8 | los otros tres |
|---|---|---|
| nivel (a −40 dB) | **+12,7 dB más alto** | iguales entre sí |
| respuesta en 20 kHz | **−4,90 dB** | −1,90 / −1,90 / −2,00 dB |
| corte implícito | **~13,8 kHz** | ~26,9 kHz |

Y acá está la prueba cruzada, que es lo que convierte la sospecha en
diagnóstico. Un polo en 13,8 kHz contra uno en 26,9 kHz predice una diferencia
de retardo de grupo de:

```
1/(2π·13800) − 1/(2π·26900) = 11,53 − 5,92 = 5,62 µs
```

**Medido por correlación: +6,05 µs.** Coinciden dentro de 0,43 µs, y son dos
mediciones que no comparten nada: una es amplitud contra frecuencia, la otra es
tiempo por correlación cruzada. **El "skew" del banco 8 es su propio filtro.**

**Qué revisar**: el más probable es la **resistencia de shunt de 1 kΩ** de ese
adaptador — si quedó más grande, sube el nivel y baja el corte, que es
exactamente el par de síntomas.

> ⚠️ **Corregido en §21 (2026-08-27).** Acá dije "si quedó de 4,7 kΩ". **No es
> 4,7 kΩ**: ese valor daría +9,7 dB y lo medido son +12,77 dB. Dos vías
> independientes apuntan a **10 kΩ** — ver §21.2 para el número y §21.3 para
> cómo confirmarlo con el téster.

> Esto es §4.2 cobrándose lo que avisaba: *"la tolerancia no importa mientras
> los adaptadores sean iguales"*. No lo son, y el error entró justo en la
> medición que el instrumento existe para hacer. **La medición relativa no
> perdona un instrumento desparejo** — pero, bien hecha, lo delata sola.

### 19.3 🔴 La ganancia del amplificador es +29 dB, no +49. Y el pasaalto está en 72 Hz, no en 723

§2.1 derivó del esquemático dos números que resultaron **equivocados por un
factor de 10** en la resistencia efectiva de ganancia. Tres mediciones
independientes, todas consistentes entre sí:

> ⚠️ **CORRECCIÓN DEL MISMO DÍA, unas horas después.** La primera versión de
> esta sección decía "**la ganancia es +29 dB**" y presentaba tres medidas que
> coincidían. **Una de las tres tenía un signo cambiado en mi cuenta**: J13 ve
> *la mitad* de la diferencial, así que la razón amp/J13 es 6 dB **más grande**,
> no más chica. Con el signo bien, esa vía da **+17 dB**, no +29. La ganancia
> exacta queda **sin cerrar** — ver §20. Lo que NO cambia es que **no son los
> +49 dB de §2.1**, y que el pasaalto está en 68 Hz.

| | §2.1 dice (R_I = 1k) | **medido** |
|---|---|---|
| ganancia | +49,07 dB | **entre +17 y +29 dB — sin cerrar (§20)** |
| pasaalto | 723 Hz | **68,0 Hz** (ajuste de 1 polo, 0,07 dB rms) |
| nivel contra J13 | +22,71 dB | **+2,70 dB** |

La respuesta del amplificador solo (canal amplificado menos J13, que es el mismo
ch24 antes del amp):

| Hz | 20 | 30 | 45 | **63** | 90 | 125 | 250 | 500 | **723** | **800** | 2000 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| dB | −10,8 | −7,9 | −5,2 | **−3,5** | −2,1 | −1,1 | −0,3 | 0,0 | **0,0** | **0,0** | 0,0 |

**−3 dB cae en ~70 Hz. En 723 Hz la respuesta es plana.** No hay ningún
pasaalto de 723 Hz en el camino real.

⚠️ El netlist **sí dice 1 kΩ** (R38/R39, verificado). O sea que el factor 10 no
está en el valor del componente sino en el modelo: lo más probable es que el
PAM8302A tenga su propia resistencia de entrada interna en serie con la externa,
y que la fórmula `2·(142k/R_I)` de §2.1 pida el total y no sólo la externa.
**Queda para confirmar en la mesa** —midiendo R38 con el téster, o contra el
datasheet del chip—, pero **el pasaalto de 68 Hz no depende de cuál sea el
mecanismo**: es una medición directa, y sola ya invalida los 723 Hz.

### 19.4 Qué queda corregido de §2.1 y §3

- **El modo 17 está bien.** §2.1 avisaba que su tono de 800 Hz trabajaba *"a
  media octava del borde de la banda"*, con −2,6 dB y 42° de corrimiento.
  **Medido: 0,00 dB en 800 Hz.** La preocupación se cae.
- **La dispersión entre canales en 800 Hz se cae con ella.** §2.1 estimaba
  ±2,8° (~10 µs) por tolerancia de los capacitores. Con el corte en 72 Hz, en
  800 Hz estamos una década arriba y la sensibilidad a tolerancias es
  despreciable — que es justamente por qué los bancos 0, 16 y 24 coinciden tan
  bien.
- ⚠️ **§3 hay que rehacerla.** "El volumen de fábrica está unos 16 dB demasiado
  alto" se dedujo con +49 dB de ganancia. Con +29 dB **sobran ~20 dB de
  margen**: a −40 dB el amplificador entrega del orden de **600 mV**, no
  3,2 V. El equipo viene corriendo muy por debajo de lo que puede.

### 19.5 Linealidad, y dónde estaría el techo

La razón entre el canal amplificado y J13 se mantiene en **+2,70 a +2,75 dB**
de −55 a −34 dB (verificado con control de datos viejos: `c4` tiene que escalar
con el volumen y se comprueba en cada punto). Sin compresión, sin recorte.

Extrapolando con la ganancia medida, el amplificador llegaría a su techo de
~3,2 V<sub>rms</sub> alrededor de **−19 dB de volumen**. **No se midió**: había
un parlante conectado y llevarlo ahí son ~4 W sobre un driver chico. Queda como
la única parte de §5.4 pendiente, y hay que hacerla **con los parlantes
desconectados**.

### 19.6 El amplificador, aislado por primera vez

J13 y el adaptador del JSPK13 miden **el mismo canal 24**, antes y después del
amplificador, **en la misma captura**. Eso da su retardo directo:

```
JSPK13 (después del amp) − J13 (antes) = +0,3867 muestras = +8,06 µs
```

Es lo que §4 había anticipado como el uso más valioso de tener las dos tomas, y
es un número que no se podía obtener de ninguna otra forma.

> Detalle que da confianza en todo lo demás: **J13 tiene sigma = 0,000 muestras
> sobre 84 informes**. El instrumento no se mueve. Los sigma de 0,03 de los
> canales amplificados son del amplificador y su adaptador, no del método.

---

## 20. La estructura de ganancia, hasta donde llega la evidencia (2026-08-26)

§3 quedó invalidada por §19.3 y hay que reemplazarla por algo. Esta sección
separa **lo medido** de **lo derivado**, porque en el medio apareció una
discrepancia que no se cierra sin un voltímetro.

### 20.1 Lo que está medido y no se discute

| | valor | cómo |
|---|---|---|
| **Pasaalto del amplificador** | **68,0 Hz** | ajuste de 1 polo, **0,07 dB rms sobre 14 puntos** |
| **Respuesta en 800 Hz** (modo 17) | **0,00 dB** | directo |
| **Apareamiento entre bancos** | **0,11 dB** en la banda (≥250 Hz) | 3 bancos, 3 barridos |
| **Razón amplificador / J13** | **+2,70 dB** | validado contra datos viejos |
| **Linealidad** | sin compresión de −55 a −34 dB | razón constante en 0,03 dB |

**Los 723 Hz de §2.1 están mal, y eso no depende de ningún modelo.** El ajuste
de un polo a 68 Hz es tan bueno (0,07 dB rms) que la forma de la curva no deja
lugar a otra lectura.

### 20.2 La discrepancia que no cierra: cuánto amplifica realmente

Dos caminos independientes hacia la ganancia, y **no coinciden**:

| camino | qué usa | resultado |
|---|---|---|
| **A** — del pasaalto | 68,0 Hz con C = 220 nF → R_I ≈ **10,6 kΩ**, y `2·(142k/R_I)` | **+28,5 dB** |
| **B** — del nivel | razón amp/J13 = +2,70 dB, divisor 1/10,4, J13 ve la mitad | **+17,0 dB** |

Once dB y medio de diferencia. **Ninguno de los dos es despreciable y no sé cuál
tiene razón.** Las dos explicaciones candidatas:

1. **El ADC carga el shunt del adaptador.** La entrada tiene **470 Ω en serie
   por pata** (§4.1), o sea ~940 Ω, que en paralelo con el shunt de 1 kΩ lo
   baja a ~484 Ω. El divisor deja de ser 1/10,4 y pasa a ~**1/20**. Con eso el
   camino B da +22,9 dB y la brecha se parte al medio.
2. **El feedback interno del PAM8302A no es 142 kΩ.** El camino A depende de
   ese número de datasheet; el pasaalto no.

> ⚠️ **Consecuencia práctica para §4.2, independiente de todo lo anterior**: si
> el ADC carga el shunt, **el adaptador atenúa ~1/20 y no 1/10,4**, o sea unos
> 6 dB más de lo diseñado. No afecta ninguna medición *relativa* —es igual en
> los cuatro— pero sí cualquier cuenta de nivel absoluto.

**Cómo se cierra**: un voltímetro en AC sobre los pines 1-2 de un JSPK, con un
tono conocido y el volumen anotado. Un solo número y las dos vías quedan
arbitradas. Es la misma medición que cierra §20.3.

### 20.3 El techo, y qué volumen usar mientras tanto

El volumen al que el amplificador empieza a recortar depende de la ganancia, que
es justo lo que no está cerrado:

| si la ganancia es | recorta con los modos de tono en |
|---|---|
| +29 dB | volumen ≈ **−22 dB** |
| +23 dB | volumen ≈ **−16 dB** |
| +17 dB | volumen ≈ **−10 dB** |

**En los tres casos el techo está MUY por encima de los −40 dB de hoy.** Lo que
sí se puede afirmar sin cerrar la discrepancia:

- **Hay como mínimo 18 dB de margen sin usar.** Verificado, no derivado: a
  −34 dB no hay ni rastro de compresión, y ése ya es 6 dB arriba del punto de
  trabajo.
- **La recomendación de §3 de bajar el default de fábrica de −24 a −40 dB se
  apoyaba en números equivocados.** Con la ganancia real, −24 dB no está
  "12 dB adentro del recorte" — en el peor de los tres escenarios está **2 dB
  por debajo** del techo, y en el mejor, 14 dB.
- **No hay razón para tocar nada hoy.** −40 dB funciona y es conservador. Si en
  algún momento hace falta más nivel acústico, está disponible.

> **Lo que esto le costó a la operación**: el equipo viene funcionando 18 dB o
> más por debajo de lo que puede, por una cuenta de esquemático que nadie
> verificó durante meses. No rompió nada —errar por lo bajo es el lado bueno
> para equivocarse— pero es exactamente el tipo de número que conviene medir
> antes de construir decisiones encima.

### 20.4 Método: dos errores míos en el mismo día, y qué los encontró

Vale anotarlo porque los dos se escaparon del mismo modo y los agarró la misma
disciplina.

1. **La referencia de diafonía** (§18.3) informó **+149 dB**. Un número
   imposible.
2. **La ganancia de §19.3**: afirmé "+29 dB" diciendo que tres medidas
   coincidían. Una tenía **un signo cambiado** — J13 ve la mitad de la
   diferencial, así que la razón es 6 dB más **grande**, no más chica. Al
   corregirlo, esa vía da +17 dB y las tres ya no coinciden.

El segundo es el peor de los dos: **no daba un número absurdo, daba uno
plausible** — y encima "confirmado" por una coincidencia que yo mismo había
fabricado. Lo encontró volver a hacer la cuenta por un camino distinto para
otra cosa. **Una coincidencia entre dos vías sólo vale si las vías son
realmente independientes**; si una se derivó de la otra, la coincidencia no
prueba nada, y en este caso yo había escrito la fórmula de la vía B para que
diera lo mismo que la A.

---

## 21. El adaptador del banco 8 sigue igual — y ahora sabemos qué valor buscar (2026-08-27)

Se repitió la medición de §19.2 para ver si el arreglo del adaptador había
surtido efecto. **No cambió nada.** Y de paso salieron dos cosas que valen más
que el "no": una cota de repetibilidad del instrumento, y **la corrección de mi
propia hipótesis** sobre qué componente está mal.

### 21.1 El resultado: idéntico, hasta el segundo decimal

Mismo punto de trabajo que §19 (modo 19, −35 dB, amplificadores encendidos),
**76 informes**:

| entrada | t medio (smp) — 2026-08-26 | t medio (smp) — **hoy** | sigma hoy |
|---|---|---|---|
| c0 · banco 0 | 513,888 | **513,891** | 0,032 |
| c1 · banco 8 | 514,179 | **514,181** | 0,029 |
| c2 · banco 16 | 513,961 | **513,963** | 0,026 |
| c3 · banco 24 | 513,967 | **513,968** | 0,008 |
| c4 · J13 | 513,580 | **513,580** | 0,000 |

Y el barrido del modo 20, 13 barridos completos:

| | 2026-08-26 | **hoy** |
|---|---|---|
| nivel de c1 sobre c0 | +12,7 dB | **+12,77 dB** |
| c1 en 20 kHz | −4,90 dB | **−4,90 dB** |
| c2 / c3 en 20 kHz | −1,90 / −2,00 dB | **−1,92 / −1,90 dB** |
| skew de c1 contra c0 | +6,05 µs | **+6,04 µs** |

Los otros tres bancos coinciden entre sí en nivel dentro de **0,04 dB**
(c2 = +0,02, c3 = −0,02 contra c0). El adaptador del banco 8 está **exactamente
como estaba**.

> **Dato lateral que sí es un resultado:** el equipo llevaba **22,5 horas
> encendido sin reiniciarse** y las cinco entradas repiten a **0,003 muestras**
> = **60 ns**. Eso no es una medición del adaptador, es una medición del
> *instrumento*, y dice que cualquier diferencia por encima de ~0,1 µs que
> aparezca en el futuro es real. J13 sigue con **sigma = 0,000**.

### 21.2 🔴 Corrección: no es una resistencia de 4,7 kΩ, es uno de 10 kΩ

§19.2 decía *"el más probable es que la resistencia de shunt de 1 kΩ haya
quedado de 4,7 kΩ"*. **Con el número de nivel en la mano, 4,7 kΩ queda
descartada**: daría +9,7 dB y lo medido es +12,77 dB.

Qué daría cada valor de la serie E24 en ese lugar:

| R shunt | nivel sobre los otros | qué mediría el téster entre los pines del RJ45 |
|---|---|---|
| **1 kΩ (correcto)** | **0 dB** | **~900 Ω** |
| 4,7 kΩ | +9,7 dB | ~3,1 kΩ |
| 6,8 kΩ | +11,3 dB | ~3,9 kΩ |
| 8,2 kΩ | +12,0 dB | ~4,4 kΩ |
| **10 kΩ** | **+12,7 dB** ← medido | **~4,8 kΩ** |
| 15 kΩ | +13,8 dB | ~5,8 kΩ |

**Y hay una segunda vía que no comparte nada con la primera.** El polo que
agrega el adaptador malo, ajustado sobre `c1 − c0` (o sea la diferencia de
respuesta, que cancela todo lo que las dos cadenas tienen en común):

```
polo de c1 − c0 = 20,4 kHz  (ajuste de 1 polo, 0,04 dB rms sobre 8 puntos)
→ R de Thévenin del adaptador malo = 4.444 Ω   (el bueno: 904 Ω)
→ R shunt implícita = 8,4 kΩ
```

**Por nivel: 6,8-10,4 kΩ. Por respuesta en frecuencia: 8,4 kΩ.** Son dos
mediciones sin nada en común —una es amplitud a 1 kHz, la otra es la forma de
la curva arriba de 2 kHz— y las dos caen sobre el mismo valor comercial:

> **Casi con seguridad entró una resistencia de 10 kΩ donde va la de 1 kΩ.**
> Que es, justamente, **el valor del diseño original de §4.2** (10k/10n) — o
> sea, un valor que está en la misma bolsa de repuestos. Es el error más fácil
> de cometer y el más fácil de no ver.

### 21.3 Cómo confirmarlo con el téster, sin ambigüedad

El canal es el del **JSPK05** (banco 8 = `c1` = **entrada2** en los informes;
mapeo medido, §19.0). En el RJ45 son los **pines 3/4 del jack 1** (§4.3).

**Con el adaptador desconectado de los dos extremos**, medir resistencia entre
esos dos pines. Se lee la shunt en paralelo con las dos series (4,7k + 4,7k):

- **adaptador sano → ~900 Ω**
- **adaptador malo → ~4,8 kΩ** (si es la de 10 kΩ)

Conviene medir **los cuatro** y compararlos entre sí: el que se sale es el que
hay que desarmar. Medirlo enchufado no sirve —la entrada del ADC y la salida
del amplificador quedan en paralelo y ensucian la lectura.

### 21.4 Qué NO hay que hacer con esto

**No corregirlo por software.** Es tentador —está medido con precisión de
0,04 dB— pero el adaptador es el *instrumento*, y un instrumento que se corrige
en el post-proceso deja de servir para descubrir cosas: la próxima anomalía real
del banco 8 quedaría absorbida por la corrección. Se arregla el fierro.

Mientras tanto, **el banco 8 no invalida nada de lo medido**: §19.1 ya se hizo
con los tres bancos sanos y el skew entre bloques SAI (1,5 µs) no lo necesita.

