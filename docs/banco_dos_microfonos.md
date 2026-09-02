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

### 4.5 Lo que sale gratis del mismo barrido

- **Nivel por parlante** → trim de ganancia por canal. `gains[]` ya existe.
- **Ancho del lóbulo en mm**, medido en vez de calculado, a cada frecuencia. Es
  el número que hay que poner en `instalacion_15_nodos.md` en lugar de la
  estimación de apertura.
- **Repetibilidad**: la misma corrida dos veces, `--comparar`. Sin eso no se
  sabe si una diferencia de 2 dB entre dos configuraciones significa algo.

---

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
| Banco físico | ⏳ en construcción — ver §3 |
| Primera medición | ⏳ espera el banco |
