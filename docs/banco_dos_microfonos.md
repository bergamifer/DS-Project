# El banco de dos micrófonos

Dos micrófonos fijos, uno enfrente del centro del muro a **1 m** y el otro
corrido, conectados a las entradas **c0** y **c1**. No se mueven entre corridas.

Este documento dice qué mide el banco, por qué la segunda entrada cambia lo que
se puede saber, cómo terminarlo, y qué medir con él. Escrito el 2026-09-02, con
el banco todavía en construcción.

---

## 1. Por qué DOS y no uno

Con un micrófono, casi todo lo que importa es inmedible.

El problema no es la precisión: es que **las preguntas del proyecto son
comparaciones entre dos lugares al mismo tiempo**, y una persona no puede estar
en dos lugares. Medir primero acá y después allá parece equivalente y no lo es:
entre las dos medidas cambia el instante de disparo, la temperatura (y con ella
la velocidad del sonido), y —sobre todo— alguien entró al campo a mover el
micrófono, que es el objeto que más refleja de la sala.

Con dos entradas, las dos medidas comparten trama TDM. No es "casi simultáneo":
es la **misma** ventana de muestreo, así que el ADC no aporta desfase entre
ellas. Lo que quede de diferencia es acústico y de nada más.

Tres preguntas abiertas del proyecto se destraban con eso:

| Pregunta | Con un micrófono | Con dos |
|---|---|---|
| **¿Cuánto cae el nivel fuera del foco?** (#25 — el contraste, el riesgo artístico) | inmedible: hay que estar en los dos lados | un cociente, en la misma ventana |
| **¿Los τ_k son del parlante o de la sala?** (#21) | dos corridas separadas, moviendo el micrófono | los mismos chirps, dos posiciones |
| **¿El eje vertical está aliaseado?** (#15 — decide gasto en DS12) | "no se percibe", a oído | el perfil vertical, en dB, a dos frecuencias |

---

## 2. Qué sabe el equipo del banco

Lo agregado el 2026-09-02, todo en la versión **v15** del protocolo:

- **`POST /tone {"hz":N,"dither":0}`** — fija la frecuencia del emisor y apaga
  el dither. Sin esto ninguna medición automática repite: los modos de tono
  llevan un LFO triangular de ±5 % en 3 Hz, y una captura de 170 ms contra un
  LFO de 333 ms cae en una fase distinta cada vez. El dither **no es un
  defecto** —emborrona las ondas estacionarias, que es lo que hace audible el
  foco— así que se apaga sólo para medir y vuelve solo al cambiar de modo.
- **AUTOFOCUS de dos micrófonos.** El scan correlaciona la captura de c0 sobre
  todo el rango de lags y la de c1 en una ventana de ±128 muestras alrededor
  del pico del primero (±91 cm de camino: 3,6× la separación del banco).
  `GET /cal` devuelve ahora **64 entradas**, marcadas con `"mic": 1|2`.
- **El foco lo sigue decidiendo c0, solo.** Los retardos que se aplican y se
  guardan en la EEPROM salen del primer micrófono y de ninguno más. El segundo
  está para **validar**, y mezclarlos convertiría una comprobación
  independiente en parte de lo que se quiere comprobar.

Herramientas:

| Qué | Para qué |
|---|---|
| `tools/perfil.py` | barre el foco y mide el nivel en los dos micrófonos: perfil del haz, ancho del lóbulo, contraste |
| `tools/calfase.py` | separa el retardo propio de cada parlante de la geometría, y compara los dos micrófonos |
| `tools/captura.py` | las ocho entradas crudas a `.wav`, para mirar cualquier otra cosa |

---

## 3. Cómo terminar el banco

### 3.1 La separación: 250 mm sirve para una cosa y no para otra

A 1 m, con la apertura del muro (600 mm en X, 480 mm en Y), el lóbulo principal
mide esto:

| Tono | λ | −6 dB en X | primer nulo en X | −6 dB en Y | primer nulo en Y |
|---|---|---|---|---|---|
| 800 Hz | 429 mm | ±357 mm | 715 mm | ±447 mm | 893 mm |
| 1200 Hz | 286 mm | ±238 mm | 476 mm | ±298 mm | 595 mm |
| 2000 Hz | 172 mm | ±143 mm | 286 mm | ±179 mm | 357 mm |

Con el segundo micrófono a **250 mm**:

- A **2 kHz está bien**: cae entre el −6 dB (143 mm) y el primer nulo (286 mm),
  o sea en la pendiente del lóbulo. El contraste sale grande. Es además el
  punto más sensible a la posición, así que hay que fijarlo bien.
- A **1,2 kHz y por debajo, no sirve**: 250 mm queda **dentro** del lóbulo
  principal (−6 dB a 238 mm ya a 1200 Hz, y a 357 mm a 800 Hz). El "contraste"
  que salga va a ser de 2 o 3 dB, y va a decir más de la pendiente local que
  del foco.
- Para la prueba de los **τ_k es débil**: dos micrófonos a 250 mm ven casi la
  misma reflexión, así que la reflexión aparece igual en los dos y el criterio
  la confunde con el parlante. A poca separación, "coinciden" es fácil de
  conseguir y significa menos.

**Recomendación: que el segundo micrófono tenga DOS posiciones marcadas en la
barra, 250 y 500 mm.** Un par de agujeros o una ranura con tope. Sigue siendo
un banco fijo —cada detente es repetible— y con eso:

- 250 mm mide contraste a 2 kHz y para arriba;
- 500 mm mide contraste de 800 Hz a 1,2 kHz, y hace fuerte la prueba de los τ_k.

Si tiene que ser un solo número, **500 mm**: pierde algo de sensibilidad en
agudos y gana todo el resto.

### 3.2 El eje: agregar la posición VERTICAL es lo que más rinde

Los dos micrófonos separados en horizontal miden el eje que **ya funciona**. La
pregunta abierta —la que decide si hay que rediseñar el muro— es la **vertical**,
y necesita que el segundo micrófono pueda ponerse *arriba* del primero.

El motivo es el paso entre filas. Las columnas están a 80 mm y las filas a
160 mm, así que el muestreo espacial es el doble de grueso en vertical:

|  | 800 Hz | 2000 Hz |
|---|---|---|
| paso X (80 mm) | 0,19 λ ✅ | 0,47 λ ✅ |
| paso Y (160 mm) | 0,37 λ ✅ | **0,93 λ — ALIASEADO** |

Por encima de **1072 Hz** (c/2d con d = 160 mm) el eje vertical pasa λ/2 y
produce lóbulos de rejilla en vez de un foco. A 2 kHz está muy pasado. Eso
explicaría por qué el 2026-08-19 el eje vertical del modo 15 "no se percibía", y
lo que hace falta para confirmarlo es medir el perfil vertical a **800 Hz** (por
debajo del límite) y a **2 kHz** (muy por encima) y ver si se comportan
distinto. Ver `hardware/DS/DS12_PLANNING.md` §0.5 e issue #15.

**Un soporte que permita poner el segundo micrófono a 250/500 mm a la izquierda
o 250/500 mm arriba convierte el banco en el instrumento que decide esa
compra.** Es una escuadra y cuatro agujeros.

### 3.3 El resto

- **c0 y c1, en ese orden.** El foco y la EEPROM salen de c0. Si quedan al
  revés, `calfase.py` lo va a delatar (la separación ajustada da con el signo
  cambiado), pero es una corrida perdida.
- **J9 es entrada de LÍNEA y no tiene bias.** Un micrófono de electret
  enchufado directo no va a sonar. Hace falta un preamplificador o una cápsula
  con alimentación propia. El conector es RJ45.
- **Los dos micrófonos, del mismo modelo y con la misma ganancia.** No hace
  falta que estén apareados —el contraste simétrico cancela la diferencia fija,
  ver §4.2— pero sí que ninguno de los dos sature.
- **Anotar la distancia real** al plano de los conos, no al frente de la caja.
  El firmware toma z desde el plano de los parlantes.
- **Absorción detrás y a los costados** si se puede. La reflexión del piso es la
  que peor sesga el pico de correlación.
- **Nivel:** subir `POST /ingain` hasta que `adc_pk` de `/status` marque unas
  100.000 cuentas con el tono puesto (≈ −38 dBFS). Lejos de saturar y muy por
  encima del piso.

---

## 4. Qué medir

Todo lo que sigue corre **desde el host, sin nadie delante de la placa**. Los
amplificadores tienen que estar encendidos: ninguna herramienta los enciende
sola.

### 4.1 Aceptación del banco — la que tiene respuesta conocida (5 min)

```
curl -X POST http://<ip>/amps -d '{"on":true}'
python3 tools/perfil.py --host <ip> --hz 2000 --sep 250 -o acept.json --grafico
```

Los dos micrófonos ven **el mismo** barrido desde posiciones distintas, así que
sus perfiles tienen que ser la misma curva **corrida por la separación física**:

```
pico(c1) − pico(c0)  ==  separación medida con cinta
```

Es la única medición del banco que puede fallar contra un número conocido de
antemano, y por eso es la primera:

- Da la separación real → **toda la cadena queda validada de una vez**: la tabla
  de geometría, el cálculo de retardos, los Q8, el mapeo de canales y el arnés.
- Da el doble o la mitad → hay un error de escala en la geometría.
- Da ~0 → **el foco no se está moviendo**; lo que cambia es el volumen, no la
  dirección. Ese resultado invalidaría todas las demostraciones de steering
  hechas a oído hasta hoy, así que conviene saberlo antes que después.

### 4.2 Contraste — el número del que depende la obra (15 min)

```
python3 tools/perfil.py --host <ip> --hz 800 1200 2000 3000 -o contraste.json
```

`instalacion_15_nodos.md` ya dice que el límite real de la instalación no es el
tamaño del foco sino **el contraste**: quince nodos sonando a la vez, y lo que
decide si se distinguen es cuánto cae cada uno fuera de su punto. Ese número
nunca se midió.

La herramienta lo mide **simétrico**: enfoca en c0 y lee cuánto más bajo está
c1, después enfoca en c1 y lee cuánto más bajo está c0, y promedia. El promedio
**cancela toda asimetría fija** —micrófonos de distinta sensibilidad, preamps
distintos, un modo de la sala en uno de los dos puntos— y por eso no hace falta
aparear los micrófonos. La diferencia entre los dos sentidos se informa aparte:
es la medida de cuán dispares son.

También informa el **piso** (una captura con los amplificadores apagados) y
avisa si el contraste queda a menos de 6 dB de él, porque en ese caso el número
es un **límite inferior y no una medida**.

### 4.3 El experimento vertical — el que decide plata (30 min)

Con el segundo micrófono **arriba** del primero (§3.2):

```
python3 tools/perfil.py --host <ip> --eje y --hz 800 --desde -600 --hasta 600 -o vert800.json
python3 tools/perfil.py --host <ip> --eje y --hz 2000 --desde -600 --hasta 600 -o vert2k.json
python3 tools/perfil.py --comparar vert800.json vert2k.json
```

Y el mismo par en horizontal, como control:

```
python3 tools/perfil.py --host <ip> --eje x --hz 800 -o horiz800.json
python3 tools/perfil.py --host <ip> --eje x --hz 2000 -o horiz2k.json
```

Las cuatro corridas contestan la pregunta con **tres desenlaces distintos**, y
los tres son resultados:

1. **Vertical a 2 kHz desordenado o con varios máximos, vertical a 800 Hz con un
   solo pico limpio** → aliasing confirmado. Bajar el paso de filas en la DS12
   se justifica, y se sabe cuánto hay que bajarlo.
2. **Vertical a 800 Hz con un solo pico pero muy ancho** (se predice ±447 mm) →
   el limitante no es el paso sino la **apertura**: 480 mm son 1,12 λ a 800 Hz.
   Achicar el paso no alcanza; hay que agregar filas. Es la respuesta cara, y es
   mejor saberla antes de fabricar.
3. **Vertical y horizontal parecidos a las dos frecuencias** → la percepción del
   2026-08-19 era otra cosa (nivel, contenido, sala) y no hay nada que rediseñar
   por este lado. Una tarde de medición que cierra una pregunta abierta vale
   igual que una que encuentra un problema.

La comparación con el eje **X** es lo que hace válido el argumento: X no está
aliaseado a ninguna de las dos frecuencias, así que sirve de control. Si X
también empeora a 2 kHz, lo que se está midiendo es la sala y no el array.

### 4.4 Los τ_k, en una sola corrida (10 min)

```
# modo 11 = AUTOFOCUS; el micrófono 1 tiene que estar en el punto focal
curl -X POST http://<ip>/mode -d '{"mode":11}'
# esperar a que /status diga cal_prog 32
python3 tools/calfase.py --host <ip> --sep 250 -o cal.json
```

El scan mide, parlante por parlante,

```
TOF_k  =  t₀  +  τ_k  +  |p_k − m| / c
          ↑     ↑        ↑
     disparo  parlante  geometría
```

y `calfase.py` ajusta por mínimos cuadrados las cuatro incógnitas `(m, t₀)`
sobre las 32 mediciones: **el residuo es τ_k**. 32 ecuaciones, 4 incógnitas.

Un ajuste con 28 grados de libertad **siempre** devuelve residuos, así que que
existan no prueba nada. Con dos micrófonos el veredicto sale de la misma
corrida: si los τ_k coinciden desde los dos puntos, son del parlante y
generalizan; si no, son sala o geometría y no hay calibración.

Y de regalo, la prueba de respuesta conocida: el ajuste estima dónde está cada
micrófono **sin que se le diga**, así que la distancia entre las dos posiciones
ajustadas tiene que dar la de la cinta métrica. Verificado sobre datos
sintéticos (`python3 tools/calfase.py --autoprueba`): recupera 250 mm con 0,3 mm
de error y 500 mm con 0,9 mm.

### 4.5 Fase de cada parlante, frecuencia por frecuencia · 5 min

```
curl -X POST http://<ip>/mode -d '{"mode":22}'      # ~3,5 minutos de tonos
# cuando /fase deje de decir "corriendo":
curl -X POST http://<ip>/mode -d '{"mode":0}'       # el muro se calla
python3 tools/fase.py --host <ip> --cal cal.json -o fase.json --mapa
```

El **modo 22** emite un tono puro por **un parlante por vez** y detecta
coherentemente en las ocho entradas: 32 parlantes × 14 frecuencias (250 Hz a
5 kHz, tercios de octava). Por cada par quedan amplitud **y fase**.

Ninguno de los modos anteriores podía dar esto:

- El **AUTOFOCUS** también recorre los parlantes de a uno, pero correlaciona un
  chirp: devuelve **un escalar** por parlante, el retardo de grupo promediado
  sobre 500 Hz–6 kHz. Si un parlante llega bien en 1 kHz y mal en 3 kHz, ese
  promedio no describe ninguna de las dos.
- El **barrido de frecuencia** (modo 20) sí da fase contra frecuencia, pero con
  los cuatro bancos sonando a la vez: mide **la suma** en el micrófono, y una
  suma no se puede desarmar — dos parlantes en contrafase dan lo mismo que dos
  parlantes mudos.

`tools/fase.py` produce cuatro cosas:

1. **Diagnóstico del muro.** Nivel de cada parlante; un mudo da piso en las 14
   frecuencias, un flojo aparece con su desvío en dB.
2. **Arribo en fase.** Restándole a cada parlante su fase geométrica
   (2π·f·d_k/c, y d_k se calcula) y después el promedio circular sobre los 32,
   queda su desviación. **La dispersión de esa desviación, frecuencia por
   frecuencia, es literalmente cuánto se desalinean** — y no hay que
   desenvolver nada, porque es una comparación a f fija. El informe la traduce
   a milímetros de camino y a costo en dB sobre la suma.
3. **¿Alcanza un retardo?** Por parlante, busca el τ_k que mejor explica su
   curva —maximizando la suma coherente sobre una grilla, no por regresión: la
   fase está envuelta y una regresión sobre ángulos envueltos no significa
   nada— y reporta lo que queda **después**. Residuo chico: es retardo puro y un
   solo número por parlante lo corrige. Residuo grande: ninguna calibración por
   retardos alcanza, y eso hay que saberlo antes de construir encima.
4. **Polaridad contra retardo, que el chirp no distingue.** Una inversión son
   π de desfase **a todas las frecuencias**; un retardo mal puesto da un desfase
   que **crece con f**. En el retardo de grupo del AUTOFOCUS los dos se ven
   parecidos; acá se separan solos.

Y hay una propiedad que lo hace válido en toda la banda: **con un solo parlante
emitiendo no hay array, así que no hay lóbulos de rejilla**. El techo de
2140 Hz que limita a los modos enfocados acá no aplica.

> **Un barrido terminado sobrevive al cambio de modo.** El uso natural es correr
> los 3 minutos, volver al modo 0 para que el muro se calle, y recién ahí bajar
> el resultado.

El análisis está verificado contra 22 casos de respuesta conocida
(`python3 tools/fase.py --autoprueba`): recupera un retardo de +150 µs con
0,3 µs de error, detecta un parlante invertido sin confundirlo con un retardo,
distingue «es retardo puro» de «no lo es», y con 10° de ruido de fase encima
sigue recuperando los retardos con 6,4 µs RMS.

### 4.6 Lo que sale gratis del mismo barrido

- **Nivel por parlante** → trim de ganancia por canal. `gains[]` ya existe.
- **Ancho del lóbulo en mm**, medido en vez de calculado, a cada frecuencia. Es
  el número que hay que poner en `instalacion_15_nodos.md` en lugar de la
  estimación de apertura.
- **Repetibilidad**: la misma corrida dos veces, `--comparar`. Sin eso no se
  sabe si una diferencia de 2 dB entre dos configuraciones significa algo.

---

## 4.7 El orden de la mañana

Los cinco pasos, en orden, con lo que cada uno decide. Total: **menos de una
hora**, y los tres primeros son de diez minutos.

| | Qué | Comando | Decide |
|---|---|---|---|
| **0** | Nivel de los micrófonos | `python3 tools/nivel.py --host <ip>` | si vale la pena seguir. Menos de 20 dB de margen sobre el piso y todo lo demás mide ruido |
| **1** | Aceptación del banco | `python3 tools/perfil.py --host <ip> --hz 2000 --sep 250 --grafico` | si la cadena entera está bien. Tiene **respuesta conocida** |
| **2** | Fase por parlante | modo 22, después `tools/fase.py` | si los 32 llegan en fase, y si un retardo por parlante alcanza |
| **3** | Contraste | `python3 tools/perfil.py --host <ip> --hz 800 1200 2000 3000` | el número del que depende la instalación de 15 nodos |
| **4** | τ_k con los dos micrófonos | modo 11, después `tools/calfase.py` | si la calibración por retardos es real o es sala |

**Antes de todo**, y a mano, porque ninguna herramienta lo hace sola:

```
curl -X POST http://<ip>/amps -d '{"on":true}'
```

**El paso 0 no es burocracia.** Con la ganancia baja, la detección coherente
mide ruido y los informes salen vacíos; con la ganancia alta, el ADC recorta y
las fases que salen son las de la distorsión. En el medio hay 40 dB de margen.

**El paso 1 va antes que el 2 por un motivo**: si el foco no se está moviendo
—si lo que cambia con la posición es el volumen y no la dirección— entonces
todo lo que diga el paso 2 sobre fases es sobre un array que no está formando
nada. El paso 1 es el único que puede fallar contra un número que se conoce de
antemano.

## 5. Límites, dichos de entrada

- **El foco es un punto, y el micrófono también.** El banco mide dos puntos de
  un campo tridimensional. Un perfil en X a z = 1 m no dice nada de lo que pasa
  a 2 m ni fuera del eje.
- **La sala está adentro de todos los números.** El contraste medido en este
  banco es el de *esta* sala a *esta* distancia. Para la instalación importa la
  tendencia con la frecuencia, no el valor absoluto.
- **La correlación del chirp da retardo de grupo**: un escalar por parlante, no
  fase contra frecuencia, y sólo en la banda del chirp (500 Hz – 6 kHz). Si los
  drivers difieren en la *forma* de su fase —probable cerca de la resonancia de
  125–180 Hz de §28— un retardo único no lo arregla, y el pipeline tampoco
  podría sostener un FIR por parlante.
- **Por encima de 2140 Hz el eje X también aliasea** (80 mm > λ/2). El firmware
  acepta pedir hasta 8 kHz y lo avisa por el serie: medir el aliasing es uno de
  los experimentos, pero el número que salga ya no describe *un* foco.
- **El detector es coherente** (Goertzel a la frecuencia exacta), no RMS. Con
  RMS el piso de la sala tapa justamente los −20 dB de contraste que se quieren
  medir.

---

## 6. Estado

| | |
|---|---|
| Firmware | ✅ v15 en la placa `192.168.100.77`, verificado: `/tone`, `/cal` con 64 entradas |
| `tools/perfil.py` | ✅ probado de punta a punta contra la placa (sin micrófonos: informa correctamente "esto es el piso del ADC") |
| `tools/calfase.py` | ✅ doble micrófono + prueba de separación, verificado con datos sintéticos |
| Modo 22 · fase por parlante | ✅ en la placa: corre, termina, sobrevive al cambio de modo, se baja entero |
| `tools/fase.py` | ✅ 22 comprobaciones de respuesta conocida |
| `tools/nivel.py` | ✅ probado contra la placa |
| Banco físico | ⏳ en construcción — ver §3 |
| Primera medición | ⏳ espera el banco |

---

## 6. Primera sesión con micrófonos — 3 de septiembre de 2026

Sala de 3 × 2,5 m, obra en curso al lado, micrófonos en c0 y c1 (ver la
advertencia de §6.4). Todo lo que sigue está medido.

### 6.1 El arnés está cruzado de a pares — confirmado con un número

La caja conectada al canal `k` del firmware está en la posición `k^1`:
0↔1, 2↔3, … 30↔31. Fernando lo avisó; la medición lo confirma sin ambigüedad.

El AUTOFOCUS mide el tiempo de vuelo de los 32 parlantes y `calfase.py --pares`
reajusta con los dos mapeos:

| mapeo | RMS del residuo | posición ajustada del micrófono |
|---|---|---|
| cableado directo | **62,0 µs** = 21,3 mm | (−293, −85, 1341) |
| **pares cruzados** | **3,9 µs** = 1,3 mm | **(−233, −52, 948)** |

**15,8× mejor**, y los **dos micrófonos por separado** dan el mismo veredicto y
bajan a los mismos 3,9 µs. El ajuste par por par dice que no falta cruzar
ninguno más: son los 16, uniformemente.

**El síntoma, para reconocerlo en otro nodo:** en la tabla de τ_k del AUTOFOCUS
los residuos **alternan de signo dentro de cada par** y **crecen hacia el borde
del muro** — porque la diferencia de camino entre columnas vecinas crece con la
distancia al micrófono. Con el cableado directo se veía así:

```
ch14 = +128 µs   ch15 = −72 µs        ch22 = +108   ch23 = −76
ch30 = +131 µs   ch31 = −66 µs        ch12 =  +66   ch13 = −78
```

Corregido en el firmware con `SPK_PAIR_SWAP` (main.c), que lo dice en el banner
del arranque. **Es un parche para un error de cableado**: cuando el arnés se
arregle hay que ponerlo en 0.

### 6.2 La prueba de aceptación pasó con 3 mm

Con el mapeo corregido, el ajuste recupera **247 mm** de separación entre los dos
micrófonos contra los **250** de la cinta métrica. Con eso quedan validados de
una vez la tabla de geometría, los retardos Q8, el mapeo de canales y el arnés.

Y ubica los micrófonos: **c1 en (+14, −46, 954) es el del centro; c0 en
(−233, −52, 948) es el de la izquierda.** El −X del firmware es la izquierda del
que mira los conos.

### 6.3 Dos cajas mudas

| canal | cono | nivel |
|---|---|---|
| ch6, ch7 | fila 0, columnas 7 y 6 | −107 dBFS |
| ch28, ch29 | fila 3, columnas 5 y 4 | −109 dBFS |

Sesenta decibeles por debajo del muro: desconectadas, no flojas. Y son
**exactamente dos pares adyacentes**, o sea dos cajas. Los otros 28 canales
tienen **2,1 dB** de dispersión entre sí, que es muy buen apareamiento.

### 6.4 ⚠️ El mapeo de las ENTRADAS se corrió un slot, una vez

Al empezar la sesión los micrófonos aparecían en **c0 y c7**; después de un
reinicio por OTA pasaron a **c0 y c1**, y los ocho canales se corrieron
exactamente una posición (`ahora[k] = antes[k−1]`, incluidos los canales
conectados-pero-callados). **Siete reinicios posteriores fueron estables**, así
que no se reproduce y no puedo decir desde acá si fue la alineación TDM del
SAI3_B o el conector.

**Lo que hay que hacer, hasta saberlo:** no dar por sentado en qué entrada está
cada micrófono. `nivel.py` los lista siempre, `fase.py` corta si el canal pedido
está en el piso, y el AUTOFOCUS ahora acepta `POST /mics {"m1":0,"m2":1}` — antes
estaban clavados en 0 y 1, y con el micrófono en c7 medía un canal vacío
**sin decir nada raro**: la calidad daba baja y parecía ruido de sala.

### 6.5 Los parlantes SÍ llegan en fase — la sala no

Es la pregunta que motivó la sesión, y tiene dos respuestas distintas según qué
se mida:

| método | qué ve | dispersión |
|---|---|---|
| **chirp** (AUTOFOCUS) | sólo la primera llegada | **3,9 µs = 1,3 mm** |
| **tono continuo** (modo 22) | directo + sala | **50–90°** a todas las frecuencias |

En el camino directo los 32 llegan alineados dentro de **1,3 mm**, que a 1 kHz
es λ/264. Lo que rompe la suma en el punto de escucha es la sala, y ningún
retardo por parlante puede arreglar eso — el análisis lo dictamina solo
(«residuo 58,7°, no es retardo puro») y la comparación con el chirp dice por qué.

Y la prueba del issue #21 pasó: los τ_k medidos desde los dos micrófonos
**coinciden** — dispersión 3,90 µs dentro de cada corrida contra 1,09 µs de
diferencia entre ellas. Son del parlante y generalizan. Con la salvedad que el
propio script advierte: a 250 mm los dos puntos ven casi la misma reflexión, así
que la prueba es **débil**. A 500 mm valdría mucho más (§3.1).

### 6.6 El contraste no se puede medir en esta sala

Sintetizando la respuesta del array desde el barrido de fase — validado contra
el perfil medido físicamente con **r = 0,95 y 0,6 dB de error RMS**:

| Hz | 250 | 500 | 800 | 1250 | 2000 | 2500 | 3150 | 5000 |
|---|---|---|---|---|---|---|---|---|
| contraste a 247 mm | −0,0 | −0,4 | +3,1 | +4,8 | +2,8 | +10,3 | +13,0 | +12,3 dB |
| ancho −6 dB medido | — | 1020 | 820 | 440 | **540** | **700** | 180 | **460** mm |
| el mismo array sin sala | — | 1400 | 880 | 520 | **320** | **240** | 180 | **100** mm |

Por debajo de 1,2 kHz el contraste es de **0 a 3 dB**, que era lo previsto en
§3.1: a 247 mm el segundo micrófono cae dentro del lóbulo principal. Pero
además, en 2000, 2500 y 5000 Hz **la sala ensancha el lóbulo entre 1,7× y 4,6×**
respecto del mismo array en campo libre.

**El número del que depende la instalación de quince nodos no se puede medir
acá.** Hace falta un espacio más grande o más muerto, o un método con ventana
temporal — el chirp ya demostró que atraviesa esta sala sin problema, y una
versión enfocada del chirp mediría el contraste del sonido directo. Es lo que
falta construir.

### 6.7 Un error propio, dicho porque cuesta caro

La síntesis del array estuvo mal en su primera versión: asumía que la cadena de
medición traduce un retardo en fase con signo negativo, y es **positivo**.
Con el signo cambiado el array **desenfoca justo donde debería enfocar**. Lo
delató la validación contra el perfil medido: r = +0,950 con el signo correcto
y **−0,389** con el otro. Ahora `signo_de_fase()` lo determina de los datos y
`sintetizar()` lo pide explícito.

---

## 7. El chirp enfocado (modo 23) — medir el foco sin la sala

Construido el 3-sep-2026 porque §6.6 cerró con «el contraste no se puede medir
acá». Con esto sí se puede.

### 7.1 Qué hace

El array **entero** emite el chirp del AUTOFOCUS (500 Hz–6 kHz, 10 ms) **con los
retardos del foco**, el micrófono correlaciona, y se toma el pico de la
**primera llegada**. Eso mide el sonido directo y descarta las reflexiones por
construcción: una reflexión llega siempre después.

No hay emisor nuevo. El chirp se escribe en `beam_line` y el camino de salida de
siempre le aplica `tx_delay_q8[]`, o sea el foco vigente. Mover el foco es un
`POST /beam` como en cualquier modo; el resultado se lee con `GET /foco`.

### 7.2 Lo que cambia, medido sobre el mismo muro y la misma sala

Mismo barrido de 21 posiciones, mismos dos micrófonos, con media hora de
diferencia:

| | tono continuo | **chirp enfocado** |
|---|---|---|
| contraste simétrico a 247 mm | +2,8 dB | **+17,7 dB** |
| asimetría entre los dos sentidos | +1,4 dB | **+0,3 dB** |
| ancho −6 dB del lóbulo | 540 mm | **241 y 267 mm** |
| pico de c1 (está en +14 mm) | −32 mm | **+17 mm** |
| pico de c0 (está en −233 mm) | +179 mm ✗ | **−240 mm** ✓ |
| separación recuperada (real 247) | +78 mm ✗ | **+258 mm** ✓ |
| tiempo del barrido | 65 s | **23 s** |

El **+0,3 dB de asimetría** es el número que más dice: enfocar en A leyendo B y
enfocar en B leyendo A dan lo mismo dentro de tres décimas. Eso sólo pasa si lo
que se mide es el haz y no la sala.

Y el ancho de 241–267 mm contra los **286 mm** que predice λz/D a 2 kHz cierra
el círculo: el array hace lo que la geometría dice que tiene que hacer.

### 7.3 Cómo se usa

```
curl -X POST http://<ip>/amps -d '{"on":true}'
python3 tools/perfil.py --host <ip> --chirp --mics 0 1 \
        --desde -600 --hasta 400 --paso 50 --sep 247 --grafico
```

`--chirp` cambia el detector y **todo el análisis de siempre sigue valiendo**:
perfil, ancho del lóbulo, contraste simétrico y la prueba de respuesta conocida
de la separación.

### 7.4 Dos trampas que costaron tiempo, anotadas

**La ganancia de entrada se aplicaba al chirp.** El modo lleva `.tone = 0`, y
`beam_out_q8 = st->tone ? 256 : in_gain_q8` le metía **+16 dB a una señal que ya
está a −6 dBFS**: 1,3 millones de muestras recortadas y una correlación que daba
calidad 37 donde el mismo chirp por un solo parlante daba 237. El recorte no se
oía —el chirp dura 10 ms— pero arruinaba la medición entera. La regla es:
ganancia unitaria cuando la fuente es interna, ganancia de entrada sólo cuando
la fuente es el micrófono.

**El umbral de la primera llegada no puede ser sólo relativo al máximo.** Con la
mitad del máximo como umbral y una medición pobre, el buscador se quedaba con el
primer grumo de ruido: un micrófono informaba una llegada 184 muestras antes que
su vecino estando los dos a 6 mm de distancia. Ahora el umbral es
`max(máximo/2, 6 × promedio)`, así una medición pobre devuelve el máximo global
—que es defendible— en vez de un accidente.

### 7.5 ⚠️ Pendiente: el ciclo se pierde ~1 de cada 10

La primera versión armaba la captura dentro de `Process_ADC_Half` cuando veía
`fc_chirp_pos == 0`, lo que ataba el armado al instante del fill; el barrido se
plantaba en un punto al azar con el estado en `CAPT` para siempre. Armar la
captura y el emisor **juntos**, desde el main loop, lo mejoró mucho pero **no lo
eliminó**: en un barrido de 21 puntos el perro guardián tuvo que reintentar 2
veces.

El contador `retries` de `GET /foco` lo hace visible. **Si crece durante un
barrido, los puntos no son comparables entre sí.** Falta encontrar la causa de
raíz.
