# Debug eléctrico por lazo cerrado (salida → entrada)

> Análisis pedido el 2026-08-20: *"¿podemos conectar una salida a la entrada
> desde J13 y analizar fase y nivel?"*
>
> **Respuesta corta: la idea es muy buena y vale la pena, pero J13 no sirve —
> no existe eléctricamente en la placa.** Acá está qué es J13 realmente, qué se
> puede usar en su lugar, y qué mediciones habilita.

---

## 1. J13 no está conectado a nada

En el esquemático (`ALL_AMPS.kicad_sch`) J13 es un header de 2 pines cableado a
**VD.P1 / VD.N1** — la salida diferencial del DAC que alimenta la entrada 1 del
banco de amplificadores A12. Como punto de prueba estaría perfecto: es la señal
de línea *antes* del amplificador.

**Pero en el PCB no llegó nada de eso.** Del `DS.kicad_pcb`:

```
J13   PinHeader_1x02_P2.54mm_Vertical   F.Cu  @(176.947, 95.928)
      attr: through_hole exclude_from_pos_files exclude_from_bom dnp
      pad 1: *** SIN RED ***
      pad 2: *** SIN RED ***
```

Tres cosas, y cada una sola ya alcanzaría:

| evidencia | qué significa |
|---|---|
| `dnp` | *do not populate*: no se suelda header |
| `exclude_from_bom` + `exclude_from_pos_files` | no está en la lista de materiales ni en el archivo de posiciones — la fábrica ni se enteró |
| pads **sin red asignada** | el ruteador nunca le llevó cobre |

Y se verificó geométricamente: **ninguna pista de cobre termina a menos de
0,6 mm de sus pads.** Son dos agujeros metalizados aislados.

### No es sólo J13: la DS11 no tiene NINGÚN punto de prueba usable

El PCB tiene además **seis test points, TP1 a TP6** — y están exactamente en la
misma situación:

```
TP1 @(145.705,  89.959)   sin red
TP2 @(145.705,  98.341)   sin red
TP3 @(145.705,  92.753)   sin red
TP4 @(145.705,  95.547)   sin red
TP5 @(200.315, 120.947)   sin red
TP6 @(197.521, 120.947)   sin red
```

Los siete footprints (J13 + TP1-6) están marcados DNP y ninguno tiene red. Son
huellas que quedaron en el diseño sin llegar a conectarse.

> **Esto es un requisito para la DS12**, no una curiosidad. Ver
> `hardware/DS/DS12_PLANNING.md`.

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

No hay punto de prueba en la placa, pero **no hace falta tocar la placa**: las
salidas de parlante son conectores JST accesibles y las entradas son RJ45. Con
un adaptador de seis componentes se cierra el lazo.

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
