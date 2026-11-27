# Guía de uso del DS11 — conexionado, encendido y carga de programa

> Para alguien que va a manipular el equipo por primera vez. **No hace falta saber
> de audio ni de electrónica**: cada paso dice qué hacer, en qué orden, y qué tiene
> que pasar si salió bien. Donde algo puede romperse, está marcado con ⚠️.
>
> Si algo no coincide con lo que ves, **parar y preguntar** antes de forzar. Es un
> prototipo hecho a mano: hay detalles que no son obvios y algunos están al revés
> de lo que uno esperaría.

---

## 1. Qué es este aparato

Es un **parlante que puede apuntar el sonido a un lugar del espacio**, como una
linterna apunta luz.

Un parlante común manda sonido para todos lados. Este tiene **32 parlantes chicos
en una grilla de 4 filas × 8 columnas**, y una computadora que le da a cada uno la
misma señal pero **con un retraso distinto**, de millonésimas de segundo.

La idea es la misma que la de una ola en una tribuna: si cada persona se levanta en
el momento justo, la ola llega a un punto determinado. Acá pasa igual con el
sonido: eligiendo bien los retrasos, todas las ondas **llegan juntas a un punto
elegido del aire** y ahí se suman. En ese punto se escucha más fuerte; alrededor,
menos.

Eso permite algo que un equipo de audio normal no hace: **poner ocho sonidos
distintos en ocho lugares distintos de una sala, al mismo tiempo, con un solo
aparato.** Caminando por la sala, uno va "entrando y saliendo" de cada sonido.

**Dos ideas que conviene tener presentes:**

- **El efecto es más fuerte cerca.** A 1 metro se nota bien; a 4 metros el sonido
  sigue estando dirigido pero ya no hay un "punto" sino una zona ancha. No es una
  falla, es física.
- **La sala pelea.** En un ambiente chico y con paredes duras, el eco tapa el
  efecto. Al aire libre o en un espacio con cortinas, alfombras o gente, se escucha
  mucho mejor.

### Las dos computadoras

El equipo tiene **dos chips programables**, y hay que entender cuál hace qué:

| | qué hace | cuándo se toca |
|---|---|---|
| **STM32** (el grande, centro de la placa) | genera todo el audio y hace las cuentas de los retrasos | rara vez |
| **ESP32** (el chiquito con antena, al borde) | se conecta al WiFi y recibe las órdenes desde el celular o la computadora | rara vez |

El ESP32 es **el que recibe las órdenes** y se las pasa al STM32. Si el ESP32 no
está en la red, **no hay forma de manejar el equipo** (más sobre esto en §6).

---

## 2. Las tres conexiones

Hay exactamente tres cosas que se enchufan. En este orden:

```
   ┌─────────────────────────────────────────────┐
   │                                             │
   │   [1] ALIMENTACIÓN      bornera de tornillo │ ← 19-20 V
   │                                             │
   │   [2] PARLANTES         16 conectores JSPK  │ ← 2 parlantes c/u
   │                                             │
   │   [3] AUDIO QUE ENTRA   4 conectores RJ45   │ ← cable de red
   │                                             │
   └─────────────────────────────────────────────┘
```

---

## 3. Alimentación ⚠️

Esta es la parte donde es más fácil romper algo.

### Qué fuente usar

- **19 o 20 voltios de corriente continua.** Un cargador de notebook típico sirve.
- ⚠️ **Esto vale para la DS11.** La DS12 (todavía no fabricada) va a 24 V: cuando convivan, las
  fuentes de 24 V tienen que estar marcadas y **nunca** conectarse a una DS11. Ver
  `hardware/DS/DS12_ENTRADA_24V.md`.
- ⚠️ **NUNCA 24 V** en una DS11. El componente que baja la tensión aguanta 26 V como máximo
  absoluto. A 24 V no queda margen ante cualquier pico y se quema. **No es una
  recomendación conservadora: es el límite del chip.**
- **Que pueda dar corriente de sobra.** Si la fuente es regulable y tiene límite de
  corriente, ponerlo **holgado** (al menos 3 A). Con el límite bajo, la placa se
  reinicia sola apenas los parlantes piden potencia — y parece un problema de
  programa cuando en realidad es la fuente. Ya nos pasó y perdimos una tarde.

### Cómo se conecta

Va a la **bornera de tornillo** de la entrada. Respetar **positivo y negativo**:
está serigrafiado en la placa al lado del conector.

> Si la placa tiene además un **conector tipo barril** soldado (las de laboratorio
> pueden tenerlo), es la misma entrada en paralelo: **usar uno o el otro, nunca los
> dos a la vez** con fuentes distintas.

### Cómo saber que está bien

Al dar tensión se encienden LEDs en la placa. Si **no se enciende nada**, o si la
fuente marca que está en límite de corriente, **cortar de inmediato** y avisar.

---

## 4. Los parlantes

### Cómo están organizados

Hay **32 parlantes** en una grilla de **4 filas × 8 columnas**. Se conectan a
**16 conectores blancos (JSPK01 a JSPK16)**, y **cada conector lleva 2 parlantes**.

Cada conector tiene **4 pines**, con esta polaridad:

```
        pin 1   pin 2   pin 3   pin 4
          +       −       −       +
        └── parlante A ──┘ └ parlante B ┘
             (1 y 2)         (4 y 3)
```

⚠️ **La polaridad importa de verdad.** Si un parlante queda al revés, no "suena un
poco distinto": **cancela** el sonido de sus vecinos y arruina el efecto de
enfoque en toda esa zona. Los cables ya vienen armados con esta convención —
**no rearmarlos sin verificar**.

### ⚠️ La trampa: JSPK09 y JSPK10 están cruzados

Este es el detalle menos intuitivo de todo el equipo, y está **en el diseño de la
placa**, no en el programa.

Los 16 conectores siguen el orden de la grilla... **salvo el 9 y el 10, que están
intercambiados entre sí.** Es un error conocido del esquemático de esta versión.

**Qué significa en la práctica:** el cable que por posición correspondería al
JSPK09 va enchufado en el JSPK10, y viceversa.

Está anotado para corregirse en la próxima versión de la placa. Mientras tanto, si
al hacer la prueba de recorrido (§7) el sonido "salta" de lugar justo en esos dos,
**es esto y no un cable suelto**.

---

## 5. El audio que entra — los conectores de red (RJ45)

Acá viene lo que más sorprende: **los conectores de red NO son para internet.**

Se usaron conectores RJ45 (los mismos de un cable de red) porque **un cable de red
tiene 4 pares de cables trenzados adentro**, y eso permite meter **4 canales de
audio por un solo cable**, con cables baratos y fáciles de conseguir. El equipo no
manda ni recibe datos de red por ahí — es audio analógico puro.

### Qué entra en cada uno

Hay **4 conectores**, pero llevan **8 canales de audio** en total:

| conectores | canales | para qué sirve el duplicado |
|---|---|---|
| **1 y 2** (están en paralelo) | entradas **1 a 4** | el segundo repite al primero, para encadenar otro equipo |
| **3 y 4** (están en paralelo) | entradas **5 a 8** | ídem |

Dentro de cada cable, los pines van así:

| pines del RJ45 | canal |
|---|---|
| 1 (+) y 2 (−) | entrada 1 (o 5) |
| 3 (+) y 4 (−) | entrada 2 (o 6) |
| 5 (+) y 6 (−) | entrada 3 (o 7) |
| 7 (+) y 8 (−) | entrada 4 (o 8) |

### ⚠️ Qué fuente de audio conectar

- **Nivel de línea**: la salida de una computadora, un celular, una consola de
  sonido, una placa de audio. **Todo lo que ya tenga su propia amplificación.**
- ⚠️ **NO conectar micrófonos comunes directamente.** La placa **no entrega
  alimentación** a los micrófonos. Un micrófono de los que necesitan alimentación
  no va a funcionar (no se rompe, simplemente no se escucha nada). Si hay que usar
  micrófono, tiene que pasar antes por una interfaz o preamplificador.

### ⚠️ Los cables tienen que ser dedicados — no sirve un cable de red común

Un cable de red trae **4 pares de cables trenzados**, y el trenzado es lo que
cancela el ruido. El problema es que **la placa agrupa los pines de una forma y el
cable estándar los trenza de otra**:

| | pares que agrupa |
|---|---|
| **La placa DS11** | (1,2) (3,4) (5,6) (7,8) |
| **Un cable de red estándar** (T568B) | (1,2) (3,6) (4,5) (7,8) |

Coinciden en dos y no coinciden en dos. Resultado: con un cable común, **las
entradas 2 y 3 de cada conector viajan por dos pares distintos** y pierden la
cancelación de ruido. Funcionan, pero toman más zumbido e interferencia del canal
vecino — y eso empeora cuanto más largo es el cable.

### ✅ La solución: cables armados a medida

Se arreglan **completamente** armando los cables, sin tocar la placa. La receta es
un cable T568B normal **con dos conductores intercambiados**:

```
        pin      T568B normal        CABLE DS  ← usar este
        ───────────────────────────────────────────────────
         1       blanco/naranja      blanco/naranja
         2       naranja             naranja
         3       blanco/verde        blanco/verde
         4       azul            →   verde          ← cambia
         5       blanco/azul         blanco/azul
         6       verde           →   azul           ← cambia
         7       blanco/marrón       blanco/marrón
         8       marrón              marrón
```

**Sólo se intercambian los conductores de los pines 4 y 6**, y hay que hacerlo
**igual en las dos puntas**. Así el cable sigue siendo pin-a-pin (1→1, 2→2, …), o
sea que cada señal llega donde tiene que llegar, pero ahora **los cuatro canales
viajan sobre pares realmente trenzados**.

⚠️ **Etiquetar estos cables.** Por fuera son idénticos a un cable de red y no lo
son. Conviene ponerles una funda de color o una etiqueta tipo **"DS AUDIO — NO ES
RED"**. Nada se rompe si se confunden, pero se pierde media hora buscando un
zumbido que no existe.

> **Si no hay cables a medida a mano**: usar sólo las **entradas 1 y 4** de cada
> conector, que son las que sí caen sobre pares trenzados en un cable estándar.
> Se pierden la mitad de los canales pero funciona bien.

En tiradas cortas —dos o tres metros sobre una mesa— con cualquier cable alcanza.
Esto importa cuando los cables cruzan una sala.

---

## 6. Encender y controlar

### El orden

1. Conectar parlantes y audio **con la placa apagada**.
2. Dar tensión.
3. Esperar unos 10 segundos a que el ESP32 se conecte al WiFi.
4. Abrir en el navegador: **`http://domosonica.local`**

El equipo **se anuncia solo en la red** con ese nombre, así que no hace falta
averiguar ninguna dirección. Funciona en Mac, iPhone y iPad sin instalar nada; en
Windows y Android a veces no, y ahí sí hay que buscar la IP (aparece en el monitor
serie o en la lista de dispositivos del router) y usar `http://LA_IP`.

> En todos los comandos de acá en adelante, donde dice `LA_IP` podés escribir
> `domosonica.local`.

### ⚠️ Primero hay que encender los amplificadores

**Al arrancar, el equipo está mudo a propósito.** Los amplificadores quedan
apagados hasta que se los enciende explícitamente. No es una falla.

Se hace desde el botón en la página web, o con este comando en una terminal:

```bash
curl -X POST http://LA_IP/amps -d '{"on":true}'
```

Está así porque los 32 amplificadores encendiéndose todos juntos daban un tirón de
corriente que reiniciaba la placa. También sirve como **botón de pánico**: para
silenciar todo al instante, el mismo comando con `false`.

### Para instalación: que se encienda solo

Arrancar mudo es lo correcto en la mesa de trabajo, pero **ata el sonido a que
haya red**: un nodo que pierde el WiFi queda mudo toda la función y no hay forma
de encenderlo sin ir hasta la placa.

Para eso está el **arranque autónomo**. Con esto activado, el nodo enciende sus
amplificadores **solo, 15 s después de arrancar, sin red y sin que nadie mande
nada**:

```bash
curl -X POST http://LA_IP/autostart -d '{"on":true}'
```

También está como casilla en la página web, al lado de los botones de
amplificadores. **Se guarda en la placa**, así que sobrevive a un corte de luz —
que es justamente el caso que cubre.

**El freno.** Si lo que reinicia el equipo es la corriente de los amplificadores,
encender solo cada vez sería un nodo rebotando toda la noche. Así que el nodo
cuenta sus intentos: si tres arranques seguidos no llegan a estabilizarse, **deja
de intentar y lo dice** (la página muestra `SUSPENDIDO` en rojo, y `/status`
informa `autostart_held`). Encender los amplificadores a mano —por red o con el
K2 largo— rearma la cuenta: si una persona los prendió y el equipo aguantó, la
sospecha quedó desmentida.

**De fábrica está apagado.** Se activa a propósito, para instalación.

### ⚠️ El volumen

**Empezar bajo siempre.** Son 32 parlantes: al mismo ajuste suenan bastante más
fuerte que un equipo chico. Un valor seguro para arrancar:

```bash
curl -X POST http://LA_IP/volume -d '{"db":-40}'
```

El número va de **0** (máximo) a **−63** (casi mudo). **−40 es un buen punto de
partida**; de ahí se sube de a poco. Si suena distorsionado, bajar.

### Si suena "roto" y bajar el volumen no lo arregla

Hay un **segundo** control de nivel, y es el que importa cuando el problema no se
va bajando el volumen. La diferencia:

- **`volume`** = cuán fuerte suena **lo que sale** hacia los parlantes.
- **`ingain`** = cuánto se amplifica **lo que entra** por los cables de audio.

Si `ingain` está muy alto, la señal se **rompe adentro del equipo**, antes de llegar
al control de volumen — y entonces bajar el volumen te da lo mismo roto, pero más
bajito. Es exactamente lo que pasaba en agosto de 2026 con una computadora
conectada: rompía al 30 % del volumen de la Mac.

```bash
curl -X POST http://LA_IP/ingain -d '{"db":0}'
```

**Con `0` funcionan bien todas las fuentes normales**: computadora, celular, mesa de
sonido, reproductor. Es el valor de fábrica y **queda guardado** — se pone una sola
vez, no hace falta repetirlo en cada encendido.

El valor **`30`** es para micrófonos, que dan una señal muchísimo más débil. Si lo
dejás en 30 con una computadora enchufada, **rompe**. Ante la duda: `0`.

> **La regla**: si distorsiona, primero probá `ingain 0`. Si con eso suena limpio
> pero bajo, subí el `volume`. Nunca al revés.

### Cambiar de modo

El equipo tiene **20 modos** de funcionamiento, numerados del **0 al 19** (tonos
de prueba, recorridos, distintos tipos de enfoque). Se cambian tocando el botón
correspondiente en la página —cada uno lleva su número adelante— o así:

```bash
curl -X POST http://LA_IP/mode -d '{"mode":1}'
```

**Cuál usar para qué está en §8.** Si sólo querés escuchar algo y que se note,
andá al **16**.

### El botón físico (K2) — si no hay WiFi, alcanza con esto

Hay un **botón en la placa** que hace dos cosas según cuánto lo apretés:

| gesto | qué hace |
|---|---|
| **Toque corto** | pasa al modo siguiente |
| **Mantener 1 segundo** | enciende / apaga los amplificadores |

**Con ese botón el equipo es operable sin red**: se enciende, se mantiene apretado
un segundo para que suene, y se toca para ir cambiando de modo. La descripción de
cada modo está en [manual_modos.md](manual_modos.md).

> 💡 Si algunos parlantes quedaron mudos después de un bajón de tensión, mantener
> apretado el botón (apagar y volver a encender los amplificadores) los revive.

---

## 7. La prueba que conviene hacer siempre primero

**El "recorrido de canales" (modo 1).** Hace sonar **un parlante por vez**, un
segundo cada uno, recorriendo los 32. La vuelta completa dura 32 segundos.

```bash
curl -X POST http://LA_IP/mode -d '{"mode":1}'
```

**Qué tiene que pasar**: el sonido recorre la grilla en orden — primero la fila de
arriba de izquierda a derecha, después la siguiente, y así.

**Qué mirar:**

| lo que pasa | qué significa |
|---|---|
| Suenan los 32 en orden | ✅ Todo bien, se puede seguir |
| Un parlante no suena | Cable suelto, mal enchufado, o parlante roto |
| Dos suenan fuera de orden | Probablemente el cruce del JSPK09/10 (§4) |
| No suena ninguno | ¿Encendiste los amplificadores? ¿El volumen no está al mínimo? |

Esta prueba valida todo el conexionado de golpe. **Si falla, no tiene sentido
seguir con nada más** — todo lo demás asume que la grilla está bien mapeada.

### Y después: ¿el enfoque funciona? (modo 16)

El recorrido prueba los **cables**. Esta prueba el **enfoque**, que es lo que
hace especial al aparato.

```bash
curl -X POST http://LA_IP/mode -d '{"mode":16}'
```

El equipo emite un tono y **mueve el punto de enfoque solo**, de izquierda a
derecha y vuelta, dando un viaje completo cada 5 segundos.

**Qué tiene que pasar**: parado en cualquier lugar frente al equipo, sin
moverte, el sonido **sube y baja de intensidad** al ritmo del recorrido — se
oye más fuerte cuando el punto pasa por donde estás.

No hace falta caminar ni comparar con nada. Si eso se percibe, el enfoque
funciona.

> El tono suena **ondulante**, como si vibrara. Es a propósito y no es una
> falla: un tono perfectamente fijo arma "zonas muertas" en la sala por rebote
> de las paredes, y uno terminaría escuchando el dibujo de la sala en vez del
> del equipo.

---

## 8. Los modos: cuál usar para qué

Son 19, pero en la práctica se usan cinco. El resto son pruebas.

| Para… | Modo | Qué hace |
|---|---|---|
| **Escuchar lo que entra por los cables** | **0** | Repite las entradas por todos los parlantes. Es el modo normal |
| **Probar el conexionado** | **1** | Un parlante por vez (§7) |
| **Mostrar el enfoque a alguien** | **16** | El punto se mueve solo, izquierda↔derecha |
| **La intervención: 8 sonidos en 8 lugares** | **15** | Cada entrada de audio suena en su propio punto de la sala (§9) |
| **Recuperar un enfoque calibrado** | **18** | Usa la medición hecha con micrófono (§10) |
| *(banco)* **Medir el equipo por cable** | **19** | LOOPBACK: no suena. Necesita adaptadores; ver `loopback_debug.md` |

Los demás son enfoques fijos en puntos distintos (7 a 13), un mapa de tonos de
prueba (14), la calibración (11) y un experimento vertical (17). Están
explicados uno por uno en [manual_modos.md](manual_modos.md).

> ⚠️ **Los modos 7 a 14, 16 y 17 generan su propio tono** y no usan lo que
> entra por los cables. Si conectaste una fuente de audio y "no se escucha",
> fijate primero en qué modo estás: el **0** y el **15** son los que reproducen
> lo que entra.

---

## 9. Poner cada sonido en su lugar — el mapa (modo 15)

Esto es **para lo que se hizo el aparato**: ocho sonidos distintos, en ocho
lugares distintos de la sala, al mismo tiempo.

1. Conectá las fuentes de audio a las entradas (§5). Con **una o dos ya
   funciona** — las entradas libres no molestan.
2. Poné el equipo en **modo 15**.
3. En la página web, bajá hasta el recuadro **MULTIFOCUS**.

Vas a ver un **mapa visto desde arriba**, como un plano: el equipo está abajo
(la barra verde) y la sala se extiende hacia arriba. Hay **ocho puntos
numerados**, uno por entrada de audio.

| en el mapa | en la sala |
|---|---|
| mover un punto a la **izquierda o derecha** | ese sonido se corre hacia ese lado |
| mover un punto **hacia arriba** | ese sonido se aleja del equipo (hasta 3 m) |
| mover un punto **hacia abajo** | ese sonido se acerca al equipo |
| el deslizador **"altura fuente"** | sube o baja ese sonido |

**Se arrastran con el dedo o el mouse y el cambio se escucha al instante.**

**Qué tiene que pasar**: si separás dos puntos en el mapa y caminás por la sala,
tendrías que ir "entrando y saliendo" de cada sonido.

> ⚠️ **Tocar el mapa cambia el modo.** Arrastrar un punto pone el equipo en
> MULTIFOCUS aunque estuvieras en otro modo — es a propósito, para que la
> página no muestre una cosa y el equipo haga otra.

> ⚠️ **La altura casi no se nota, y es un límite del aparato, no un error.**
> Las filas de parlantes están demasiado separadas entre sí para enfocar bien
> en vertical: se corrige en la próxima versión de la placa. Izquierda-derecha
> y cerca-lejos sí funcionan.

---

## 10. Calibrar con un micrófono (modo 11)

**Cuándo sirve**: cuando querés que el enfoque sea exacto **en un punto
concreto** —donde se va a sentar la gente, por ejemplo— en vez de calculado
sobre el papel. El equipo mide la sala de verdad y se corrige solo.

**Qué hace falta**: un micrófono **con preamplificador o interfaz de audio**
(§5: la placa no alimenta micrófonos), enchufado a la **entrada 1**.

### Los pasos

1. Poné el micrófono **exactamente donde querés el enfoque**. No se mueve
   durante toda la medición.
2. En la página, buscá el recuadro **AUTOFOCUS**. Tiene cinco renglones con un
   punto de color al lado.
3. **Esperá a que los tres primeros estén en verde.** Si alguno está rojo, te
   dice qué falta.
4. Tocá **"Iniciar scan"**.
5. Dura unos **8 segundos** y vas a escuchar los parlantes sonando de a uno.
   **No muevas el micrófono.**

### Cómo se lee el resultado

| renglón | verde significa |
|---|---|
| amplificadores | están encendidos |
| micrófono en la entrada 1 | llega señal de verdad, no sólo ruido |
| nivel del micrófono | ni muy bajo ni saturado |
| scan | terminó |
| resultado | midió bien (**16 de 32 parlantes o más**) |
| calibración guardada | quedó grabada en el equipo |

**Si el resultado sale rojo, no pasa nada malo**: el equipo **descarta** la
medición y deja todo como estaba. Corregís lo que marque en rojo y volvés a
tocar "Iniciar scan".

> 💡 Si no hay micrófono conectado, el resultado da **0 de 32**. Eso es
> correcto: significa que no midió nada.

### Lo que queda guardado

La calibración **sobrevive a apagar el equipo**. Se guardan dos cosas:

- **La corrección de cada parlante** (si alguno está conectado al revés): se
  aplica **siempre**, en todos los modos, sin que tengas que hacer nada.
- **El punto enfocado**: se recupera con el **modo 18**, cuando lo quieras.

Están separadas porque la primera vale siempre y la segunda vale **sólo para
ese punto** — el lugar donde estaba el micrófono. Si el equipo aplicara el punto
solo, estaría enfocando hacia un micrófono que ya no está ahí.

---

## 11. Volver todo a cero

Si el equipo quedó en un estado raro —enfoques movidos, volumen imposible, no
suena como esperabas y no sabés qué tocaste— hay un **reset de fábrica** al
final de la página web.

Deja volumen en −24 dB, ganancia de entrada en 0, modo 0 y todos los enfoques
en su lugar original. **Borra también la calibración con micrófono**, así que
si hiciste una, hay que repetirla.

**No apaga los amplificadores** y no toca el conexionado. Es seguro: lo peor que
puede pasar es tener que volver a acomodar los puntos del mapa.

```bash
curl -X POST http://LA_IP/reset
```

---

## 12. Cargar el programa en las placas

Sólo hace falta al actualizar el programa. Son **dos chips independientes** y cada
uno se carga distinto.

⚠️ **Los dos van juntos** cuando el cambio toca el formato con el que se hablan.
No hace falta acordarse de cuándo: la tarjeta **Firmware** de la página compara
las versiones de los dos y **avisa cuál falta actualizar**. Si hay que hacer los
dos, el orden es **primero el STM32, después el ESP32** — el ESP32 es el que
lleva la carga hasta el STM32, así que conviene dejarlo estable hasta el final.

### El STM32 (el chip grande) — por WiFi

**Es la forma normal desde ahora.** No hace falta programador ni desarmar nada:
al final de `http://domosonica.local` hay una tarjeta **Firmware del STM32** con
un selector de archivo. Se elige `firmware/build/ds11_beamforming.bin`, se
aprieta *Actualizar* y listo. Desde la computadora es lo mismo con un comando:

```bash
cd ~/STM32CubeIDE/DS-Firmware/firmware
make -j4              # compila
make ota              # sube por WiFi
```

Durante los ~15 segundos que dura, **el equipo se queda mudo** y los
amplificadores se apagan solos; después se reinicia con el programa nuevo. Si
algo falla a mitad de camino no pasa nada: el programa que estaba sigue
intacto y se puede reintentar. Los detalles están en
[ota_wifi.md](ota_wifi.md).

> Esto necesita que la placa tenga el **bootloader** instalado. Se hace una sola
> vez, con ST-Link: `make flash-todo`. Si nunca se hizo, hay que empezar por ahí.

### El STM32 — por cable (primera vez, o rescate)

Necesita un **programador ST-Link** conectado al conector **J7**.

```bash
cd ~/STM32CubeIDE/DS-Firmware/firmware
make -j4              # compila
make flash-stlink     # carga la aplicación (0x08020000)
make flash-todo       # bootloader + aplicación — instalación desde cero
```

Hace falta cuando el equipo no arranca o no aparece en la red — es decir,
cuando no hay forma de llegarle por WiFi.

⚠️ **Dos advertencias importantes con el ST-Link:**

1. **El pin 1 del cable tiene que estar cortado.** En esta versión de placa, ese
   pin intenta alimentar todo el equipo desde el programador, que no puede, y se
   apaga solo. Si al enchufar el ST-Link **se le apaga el LED**, es esto. Los
   cables preparados para este equipo ya lo tienen cortado.
2. **Después de cargar, desconectar el ST-Link y reiniciar la placa.** Con el
   programador conectado, a veces el equipo se queda colgado al arrancar.

### El ESP32 (el chiquito con antena) — por WiFi

También se actualiza sin cable, desde la misma tarjeta **Firmware** de
`http://domosonica.local`, o con `make ota-esp`. Tarda unos 5 segundos y el chip
se reinicia; perdés la página unos 15 s mientras vuelve.

**Si la imagen nueva no logra conectarse al WiFi en 90 segundos, el equipo
vuelve solo a la anterior.** No hay forma de dejarlo sin red por una carga
fallida.

> Necesita la tabla de particiones nueva, que se instala una sola vez por cable
> (ver [ota_wifi.md](ota_wifi.md) §9).

### El ESP32 — por cable (primera vez)

Se carga por su propio **conector USB-C** (J3), sin programador.

```bash
cd ~/STM32CubeIDE/DS-Firmware/firmware_esp32c3
. ~/esp/esp-idf/export.sh
idf.py -p /dev/cu.usbmodemXXXX flash
```

El puerto (`usbmodemXXXX`) aparece al enchufar el USB. Para verlo:
`ls /dev/cu.usb*`.

### Las redes WiFi

Están guardadas dentro del programa, en un archivo que **no se sube al repositorio**
(tiene las contraseñas). El equipo prueba las redes configuradas hasta que una
funciona. Para agregar una red nueva hay que editar ese archivo y volver a cargar
el ESP32 — pedírselo a Fernando.

---

## 12 bis. Revisar el equipo sin saber de electrónica

En `http://domosonica.local` hay una tarjeta **Diagnóstico** con un botón
*Revisar el equipo*. Da una línea por cosa, en verde o rojo, **con el número que
la respalda**:

```
✓ DAC0               FORMAT ok, atenuacion -24.0 dB
✓ DAC1               FORMAT ok, atenuacion -24.0 dB
✓ DAC2               FORMAT ok, atenuacion -24.0 dB
✓ DAC3               FORMAT ok, atenuacion -24.0 dB
✓ ADC AK5538VN       TDM256 y group delay corto confirmados
✓ EEPROM             presente, layout v6
✓ SYNC entre bancos  1 us
```

No es la telemetría de siempre: **le pregunta a cada chip qué tiene adentro**.
Un DAC que perdió su configuración sigue reportando cero errores y consumo
normal — pero saca ruido. Eso no lo ve ningún contador; hay que ir a
preguntarle.

Conviene apretarlo **al llegar**, antes de dar por sentado que todo está bien, y
**después de actualizar el programa**.

---

## 13. Si algo no anda

| Síntoma | Causa más probable | Qué hacer |
|---|---|---|
| No suena nada | Los amplificadores arrancan apagados | `POST /amps {"on":true}` |
| No suena nada, y los amps están encendidos | Volumen al mínimo | Subir a −30 |
| Se reinicia al subir volumen | Fuente con poca corriente | Subir el límite de la fuente |
| Un parlante mudo | Cable | Correr el recorrido (§7) y ubicarlo |
| Dos parlantes fuera de lugar | Cruce JSPK09/10 | Es conocido, ver §4 |
| No aparece en la red | El ESP32 no conectó | Reiniciar; si sigue, la red no está cargada |
| El programador se apaga al enchufarlo | Pin 1 del cable sin cortar | Usar el cable preparado |
| Suena distorsionado y fuerte | Está saturando | Bajar el volumen |
| Distorsiona y bajar el volumen NO lo arregla | La ganancia de entrada está alta | `ingain 0`, ver §6 |
| Zumbido de fondo | Entradas de audio al aire | Desconectar lo que no se use, o silenciar |
| Conecté audio y sale un tono raro | Estás en un modo que genera su propio tono | Modo **0** o **15**, ver §8 |
| El enfoque no se mueve en altura | Límite del aparato, no una falla | Ver §9 |
| La calibración da 0 de 32 | No hay señal del micrófono | Revisar el checklist del AUTOFOCUS, §10 |
| Quedó en un estado raro y no sé qué toqué | — | Reset de fábrica, §11 |

### Reglas de oro

1. **Nunca más de 20 V.**
2. **Siempre empezar con el volumen bajo.**
3. **Ante un ruido raro y fuerte, apagar los amplificadores primero** y después
   pensar: `curl -X POST http://LA_IP/amps -d '{"on":false}'`.
4. **Ante la duda, preguntar.** Es un prototipo, no un producto.

---

## 14. Para seguir leyendo

| Documento | Qué tiene |
|---|---|
| [manual_modos.md](manual_modos.md) | Los 20 modos, uno por uno |
| [ds11_bringup_checklist.md](ds11_bringup_checklist.md) | Checklist de primer encendido |
| [ds11_errata.md](ds11_errata.md) | Fallas conocidas de esta tirada de placas |
| [protocolo_medicion.md](protocolo_medicion.md) | Cómo medir el enfoque en serio |
| [reporte_2026-08.md](reporte_2026-08.md) | Historia del proyecto y por qué cada decisión |
