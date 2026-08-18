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
- ⚠️ **NUNCA 24 V.** El componente que baja la tensión aguanta 26 V como máximo
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
4. Averiguar la dirección IP del equipo (aparece en el monitor serie, o en la lista
   de dispositivos del router).
5. Abrir esa dirección en el navegador: **`http://LA_IP`**.

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

### ⚠️ El volumen

**Empezar bajo siempre.** Son 32 parlantes: al mismo ajuste suenan bastante más
fuerte que un equipo chico. Un valor seguro para arrancar:

```bash
curl -X POST http://LA_IP/volume -d '{"db":-40}'
```

El número va de **0** (máximo) a **−63** (casi mudo). **−40 es un buen punto de
partida**; de ahí se sube de a poco. Si suena distorsionado, bajar.

### Cambiar de modo

El equipo tiene **16 modos** de funcionamiento (tonos de prueba, recorridos,
distintos tipos de enfoque). Se cambian desde la página web, o así:

```bash
curl -X POST http://LA_IP/mode -d '{"mode":1}'
```

También hay un **botón físico (K2)** en la placa que va pasando de modo en modo —
útil si no hay WiFi. La descripción de cada modo está en
[manual_modos.md](manual_modos.md).

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

---

## 8. Cargar el programa en las placas

Sólo hace falta al actualizar el programa. Son **dos chips independientes** y cada
uno se carga distinto.

⚠️ **Los dos van juntos.** Se comunican con un formato que tiene número de versión;
si se actualiza uno solo, el equipo responde a los comandos pero la información de
estado deja de leerse.

### El STM32 (el chip grande)

Necesita un **programador ST-Link** conectado al conector **J7**.

```bash
cd ~/STM32CubeIDE/DS-Firmware/firmware
make -j4              # compila
make flash-stlink     # carga
```

⚠️ **Dos advertencias importantes con el ST-Link:**

1. **El pin 1 del cable tiene que estar cortado.** En esta versión de placa, ese
   pin intenta alimentar todo el equipo desde el programador, que no puede, y se
   apaga solo. Si al enchufar el ST-Link **se le apaga el LED**, es esto. Los
   cables preparados para este equipo ya lo tienen cortado.
2. **Después de cargar, desconectar el ST-Link y reiniciar la placa.** Con el
   programador conectado, a veces el equipo se queda colgado al arrancar.

### El ESP32 (el chiquito con antena)

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

## 9. Si algo no anda

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
| Zumbido de fondo | Entradas de audio al aire | Desconectar lo que no se use, o silenciar |

### Reglas de oro

1. **Nunca más de 20 V.**
2. **Siempre empezar con el volumen bajo.**
3. **Ante un ruido raro y fuerte, apagar los amplificadores primero** y después
   pensar: `curl -X POST http://LA_IP/amps -d '{"on":false}'`.
4. **Ante la duda, preguntar.** Es un prototipo, no un producto.

---

## 10. Para seguir leyendo

| Documento | Qué tiene |
|---|---|
| [manual_modos.md](manual_modos.md) | Los 16 modos, uno por uno |
| [ds11_bringup_checklist.md](ds11_bringup_checklist.md) | Checklist de primer encendido |
| [ds11_errata.md](ds11_errata.md) | Fallas conocidas de esta tirada de placas |
| [protocolo_medicion.md](protocolo_medicion.md) | Cómo medir el enfoque en serio |
| [reporte_2026-08.md](reporte_2026-08.md) | Historia del proyecto y por qué cada decisión |
