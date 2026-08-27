# Instalación de 15 nodos: qué se puede lograr, y qué lo limita

> Escrito 2026-08-26, a pedido de Fernando: *"¿qué tipo de focalización vamos a poder
> lograr con 15 sistemas? ¿podemos producir 8 focos con suficiente resolución? ¿cuál es
> la mejor distribución en una sala de 10 × 10 m?"*
>
> Complementa [`multinode.md`](multinode.md), que resolvió **cómo** coordinar los nodos
> (geometría / reloj / control). Esto responde **cuánto rinde** el resultado, con los
> números medidos hasta hoy.

---

## 0. Qué cambió desde la evaluación de julio

`multinode.md` §6 dio "viable con confianza alta" el 2026-07-05 sobre estimaciones.
Cuatro de esas estimaciones ya son mediciones, y **todas mejoraron**:

| Supuesto de julio | Medido | Dónde |
|---|---|---|
| Dispersión de latencia entre nodos "hasta medio buffer (ms)" | **0,21 mm** tras el arranque determinístico (era 29,8 mm antes de arreglarlo) | `loopback_debug.md` §8-§10 |
| SYNC entre bancos ≈ 4 µs (medido por software) | **0,12 µs** dentro de un SAI, **1,5 µs** entre SAI — eléctrico | §19.1 |
| Pasaalto del amplificador en 723 Hz | **68 Hz** — la banda grave está disponible | §19.3 |
| "SPL 2,5 W/canal = niveles íntimos" (riesgo #4) | El equipo venía corriendo **≥18 dB por debajo** de su techo | §20.3 |

**Ninguna medición empeoró el pronóstico.** El riesgo #4 queda parcialmente resuelto: hay
mucho más nivel disponible del que se venía usando.

---

## 1. La geometría de partida

| | |
|---|---|
| Apertura de un muro | **600 × 480 mm** (8 col × 80 mm + 40 de ladrillo ; 4 filas × 160 mm) |
| Techo por aliasing horizontal | **2144 Hz** (c/2d, d = 80 mm) |
| Techo por aliasing vertical | **1072 Hz** (d = 160 mm) ⚠️ la mitad |
| Parlantes totales, 15 nodos | 480 |

El paso vertical de 160 mm es el doble del horizontal: **el eje vertical aliasea una
octava antes**. Es lo que el modo 17 existe para verificar (§0.5 de `DS12_PLANNING`).

## 2. Tamaño del foco

Semiancho del foco = **λ·z / 2L**. Con un solo muro (L = 0,6 m) a 3 m:

| f | horizontal | vertical |
|---|---|---|
| 800 Hz | ±1072 mm | ±1340 mm |
| 1 kHz | ±858 mm | ±1072 mm |
| 2 kHz | **±429 mm** | ±536 mm |

**Un muro solo no enfoca: ilumina.** A 3 m su "foco" es del tamaño de una persona
entera, en el mejor caso.

Con 15 muros repartidos, la apertura sintética es la que manda. Foco a 5 m:

| f | L = 4 m | L = 7 m | L = 10 m |
|---|---|---|---|
| 800 Hz | ±268 mm | ±153 mm | ±107 mm |
| 1 kHz | ±214 mm | ±123 mm | **±86 mm** |
| 2 kHz | ±107 mm | ±61 mm | **±43 mm** |

**Ahí sí hay foco**: del tamaño de una cabeza a 1 kHz, de un puño a 2 kHz. La apertura
sintética es el único parámetro que importa para el *tamaño* — y sale gratis, sólo hay
que separar los muros.

---

## 3. 🔴 El límite real: no es el tamaño del foco, es el CONTRASTE

Acá está el hallazgo que cambia el plan.

Un array de **K grupos compactos e irregulares** tiene un piso de lóbulos laterales de
≈ **1/K**. No importa cuántos parlantes tenga cada grupo: a frecuencias donde el grupo
es chico comparado con λ, los 32 parlantes de un muro se comportan como **un** elemento.

    15 muros  ->  piso ≈ -11,8 dB

Con **N focos simultáneos**, en cada foco entra la fuga de los otros N−1. El contraste
útil colapsa rápido. Sumando la directividad propia del muro (que sólo aparece cuando el
muro mide más de ~1 λ, o sea arriba de ~600 Hz):

| f | muro solo | piso total | contraste con **8 focos** | |
|---|---|---|---|---|
| 500 Hz | casi omni | −11,8 dB | **+3,3 dB** | inutilizable |
| 800 Hz | ~3 dB | −14,8 dB | **+6,3 dB** | marginal |
| 1 kHz | ~3 dB | −14,8 dB | **+6,3 dB** | marginal |
| 1,5 kHz | ~8 dB | −19,8 dB | **+11,3 dB** | usable |
| 2 kHz | ~13 dB | −24,8 dB | **+16,3 dB** | usable |

> **La banda donde 8 focos simultáneos funcionan es 1,5 – 2,1 kHz.**
> Menos de media octava — y su techo es el aliasing del paso de 80 mm.

Debajo de 1 kHz los ocho focos se funden: la energía llena la sala. **No es un problema
de potencia ni de sincronismo — es geometría, y no se arregla con firmware.**

### Las cuatro salidas, en orden de costo

1. **Multiplexar en el tiempo en vez de en el espacio.** Un foco por vez, alternando.
   Contraste completo (+25 dB), cero hardware nuevo. Si la obra tolera que los ocho
   puntos no suenen *exactamente* a la vez, ésta es la respuesta y es gratis.
2. **Enfocar sólo la banda alta y dejar los graves difusos.** La localización auditiva
   está dominada por 1-4 kHz: si el contenido de 1,5-2 kHz está enfocado, el oyente
   percibe la fuente ahí aunque los graves llenen la sala. Es la salida perceptual, y
   cuesta un filtro.
3. **Más grupos, más separados.** El piso es 1/K. Pasar de 15 a 30 unidades —aunque
   sean de 16 parlantes— lleva el piso a −14,8 dB y el contraste de 8 focos a +6,3 dB
   antes de sumar directividad. **Más unidades chicas y dispersas rinden más que menos
   unidades grandes.**
4. **Bajar el paso en la DS12.** Para llevar el techo de aliasing a 4 kHz hace falta
   **43 mm** de paso; a 3 kHz, **57 mm**. Ensancha la banda usable de media octava a
   más de una.

### Y lo que NO es limitante

**El nivel.** 480 parlantes en fase dan hasta **+54 dB** de presión sobre uno solo.
Sumado a los ≥18 dB de margen que aparecieron en §20.3, el SPL no es la restricción.
La restricción es el contraste.

---

## 4. Distribución en una sala de 10 × 10 m

### Lo que más importa, en orden

1. **Separar al máximo.** El tamaño del foco es λ·z/2L: la apertura sintética es lo
   único que lo mejora, y es gratis. Muros en el perímetro > muros en una pared.
2. **Irregular, nunca en grilla.** Un array periódico produce **lóbulos de rejilla**
   (copias del foco en otros puntos de la sala). Posiciones irregulares convierten esos
   lóbulos en un piso difuso. **Desalinear a propósito**: separaciones distintas entre
   vecinos, alturas ligeramente distintas.
3. **Rodear, no encarar.** Con muros en una sola pared no hay resolución en profundidad:
   el "foco" es una hoja vertical, no un punto. Con muros en al menos tres lados el foco
   se cierra en 3D.
4. **Altura y cabeceo.** Muros a **2,6 – 3,2 m** (arriba de las cabezas), cabeceando
   **20-35°** hacia abajo, con el plano focal a altura de oído (~1,6 m de pie). Resuelve
   la sombra acústica de los cuerpos, que era la preocupación correcta.

### Una disposición concreta para empezar

- **12 muros en el perímetro** a ~3 m de altura, con separaciones **irregulares**
  (p.ej. 2,1 / 2,8 / 2,4 / 3,1 m) — nunca todas iguales.
- **3 muros en el techo**, hacia el centro. Son los que dan resolución en profundidad
  para focos en el medio de la sala, que es donde el perímetro solo rinde peor.
- Cables por perímetro de techo: coincide con el tendido del daisy-chain analógico.

> El daisy-chain analógico es un aliado del ruteo: la señal va de nodo a nodo, así que
> el tendido natural **es** el perímetro. No hace falta estrella desde un punto central.

### La sala misma pesa más de lo que parece

Una sala de 10 × 10 × 4 m son 400 m³. Con superficies duras, RT60 fácilmente 1,0-1,5 s.
**El campo reverberante es difuso por definición: no se enfoca.** Todo lo que rebota
llega de todas partes y llena los nulos que el array construye.

- Con RT60 = 1,0 s, la distancia crítica de una fuente omni son ~1,1 m. La directividad
  del array la empuja lejos (≈ ×10 con 20 dB de directividad), pero **el contraste
  medido en la sala nunca va a ser el contraste del array en cámara anecoica.**
- **Tratar la sala rinde más que agregar muros.** Bajar RT60 de 1,2 a 0,5 s es más
  barato que pasar de 15 a 30 nodos, y actúa sobre el mismo síntoma.
- Prioridad de tratamiento: **techo y la pared opuesta a cada muro**. El piso lo
  absorbe el público.

---

## 5. El bafle: 12 mm de fenólico con agujeros CNC

Lo que más afecta la precisión del array, en orden:

1. **⚠️ Celdas selladas individuales por parlante.** Si los 32 comparten volumen, cada
   cono carga a los demás por el aire: mover uno mueve los otros. Eso es **diafonía
   acústica** y arruina el control independiente de cada elemento, que es la premisa del
   beamforming. Una grilla de tabiques (tipo huevera) que selle cada parlante en su
   propia celda. Además **rigidiza el panel**, que es el punto 4.
2. **Achaflanar la parte de ATRÁS de cada agujero.** 12 mm de espesor detrás de un
   parlante chico es un tubo corto: resuena y colorea. Un chaflán a 45° en la cara
   trasera lo elimina; es una pasada más de CNC.
3. **Redondear los bordes exteriores del bafle** (radio ≥ 10 mm). A 2 kHz λ = 172 mm y
   el bafle mide 600 mm: los bordes vivos difractan y meten ripple en la respuesta —
   distinto en cada muro según su recorte. Es error de amplitud que el AUTOFOCUS no
   corrige porque depende del ángulo.
4. **Sellar cada parlante contra el bafle** con burlete. Una fuga chiflando cambia la
   fase en graves, que es justo lo que se está tratando de controlar.
5. **Parlante enrasado con la cara frontal**, no montado por detrás dejando un pozo. El
   pozo es una cavidad resonante delante del cono.
6. **Relleno liviano** en cada celda (guata, no lana comprimida) para matar las
   resonancias internas de la celda.

> Con parlantes reciclados hay un punto más, ya identificado como riesgo #2 en julio:
> **la dispersión de sensibilidad entre unidades**. La polaridad ya está medida; falta
> el trim de nivel por canal. El AUTOFOCUS mide amplitud además de retardo — ese dato ya
> se está capturando y sólo falta usarlo para ecualizar.

---

## 6. Qué se puede probar YA, con 3 placas y 32 parlantes

Faltan 64 parlantes para completar tres muros. **No hacen falta para las pruebas que más
valen**, porque las preguntas abiertas son de *coherencia*, no de potencia.

### Prueba A — coherencia entre nodos (2 parlantes, 1 micrófono) 🔴

**La que más vale de todas.** Toda la arquitectura descansa en una afirmación que nunca
se verificó: que la distribución analógica se auto-sincroniza (`multinode.md` §2).

1. Un parlante en la placa A, uno en la placa B, separados ~1 m.
2. Misma señal analógica a las dos por el daisy-chain.
3. Micrófono fijo, equidistante.
4. **Barrer el retardo comandado en B** y medir el nivel en el micrófono.

Si son coherentes aparece un patrón de interferencia limpio con **nulos profundos**
(>15 dB). Si derivan, los nulos se rellenan. **La profundidad del nulo es la medida de
coherencia**, y sale con dos parlantes.

### Prueba B — estabilidad en el tiempo
La misma, sostenida 30-60 min. La premisa dice que el error **no se acumula** (0,8 µs
por deriva de latencia, no integra). Si el nulo se mantiene, la premisa queda probada.
**Si se degrada, toda la arquitectura de reloj hay que repensarla** — y es mejor
saberlo con 3 placas que con 15.

### Prueba C — a través de reinicios
`dma_phase` ya viaja en `/status` (protocolo v10). Reiniciar ambas placas N veces,
anotar `dma_phase` de cada una y re-medir el nulo. Verifica el arranque determinístico
a nivel multi-nodo, que hoy sólo está verificado dentro de una placa.

### Prueba D — el piso de lóbulos laterales con K=3 🔴
**La que decide si 15 alcanzan.** El modelo de la §3 predice piso ≈ 1/K. Con 3 grupos
predice **−4,8 dB**. Medirlo: formar un foco con los tres grupos y recorrer la sala con
el micrófono mapeando el nivel fuera del foco.

- Si sale −4,8 dB, el modelo vale y **la predicción de −11,8 dB para 15 es confiable** —
  con ella se decide entre las cuatro salidas de la §3.
- Si sale mucho mejor, el modelo es pesimista y 15 rinden más de lo previsto.

Es extrapolación de un modelo de una sola variable, validado en el punto que se puede
medir. **Cuesta una tarde y decide una compra de 12 sistemas.**

### Prueba E — aliasing vertical (modo 17)
Ya estaba en la lista y sigue siendo la que decide el paso de la DS12. Ahora además se
sabe que **a 800 Hz el amplificador está plano** (§19.3), así que el experimento mide lo
que dice medir.

> **Cómo repartir los 32 parlantes**: no 10/11/11 parejo. Para la prueba D conviene
> **tres grupos lo más compactos posible y lo más separados entre sí** — es la geometría
> que replica, a escala, lo que van a ser los 15. Para las pruebas A-C alcanzan 2.

---

## 7. Monitoreo centralizado

**No agregar servidor a los ESP32.** Ya sirven `/status` con todo lo necesario; el
trabajo va afuera.

**Arquitectura**: un *colector* (RPi o laptop en la sala) que sondea `/status` de los 15
nodos cada 1-2 s, guarda un buffer circular y sirve **una** página web.

Campos que ya existen y sirven de salud: `underruns`, `sai_errors`, `adc_ovf`,
`out_clip`, `dma_phase`, `cpu_fills_pct`, `stm32_uptime_ms`, `fw_id`, `ota_state`.

### Las alarmas que valen

| Alarma | Por qué |
|---|---|
| 🔴 **`stm32_uptime_ms` bajó** | Reinicio inesperado. Con el historial de brown-out del buck (`ds11_errata.md`, C290 removido) es **la alarma más valiosa del sistema**: detecta el problema conocido, en vivo, en el nodo exacto |
| 🔴 **`fw_id` distinto entre nodos** | Alguien quedó sin actualizar. En 15 nodos esto pasa, y produce síntomas incomprensibles |
| ⚠️ `dma_phase` ≠ 0 | Ese nodo arrancó en un estado de fase distinto → error de fase contra los demás |
| ⚠️ `underruns` creciendo | El pipeline no llega |
| ⚠️ Sin respuesta > 5 s | Caída de WiFi o de alimentación |

> El colector **no debe estar en el camino del audio**. Si se cuelga, la instalación
> tiene que seguir sonando. Con la distribución analógica eso ya es así por diseño —
> conviene no perderlo agregando dependencias.

---

## 8. Red: direcciones y arranque

### La recomendación

**DHCP con reserva por MAC en el router**, no IP estática en el firmware.

- Estática en firmware: hay que compilar distinto por nodo, y al cambiar de router se
  rompe todo. Con 15 nodos es una fuente de error garantizada.
- Reserva por MAC: el nodo pide DHCP normal y **siempre recibe la misma IP**. Cero
  configuración por nodo, direcciones estables, y se cambia en un solo lugar.
- **mDNS encima** (`ds-<device_id>.local`) para que colector y app no necesiten lista de
  IPs. Ya está en uso: el `make ota` apunta a `ds-1e1f30.local`.

### Robustez al apagar y encender

1. **Reconexión con backoff y jitter aleatorio.** Al volver la luz, 15 ESP32 se asocian
   al mismo tiempo — tormenta de asociación. Un retardo aleatorio de 0-5 s lo resuelve.
2. **Sin orden de arranque.** Ningún nodo debe depender de que otro esté vivo.
3. **El audio no depende de la red.** Ya es así (daisy-chain analógico) y es la decisión
   de arquitectura más valiosa del proyecto para una instalación. Conservarla.
4. **Última configuración en EEPROM.** Un nodo que arranca sin red tiene que quedar en
   un estado sensato, no mudo ni a volumen de fábrica. La persistencia ya existe.
5. **AP dedicado para los nodos**, SSID propio, **2,4 GHz** (el ESP32-C3 no tiene 5 GHz),
   canal fijo y despejado. Separado de la red de la sala.

### Turnos para medir 480 parlantes

Para el auto-survey hay que hacer emitir a cada parlante por turno sin pisarse:

- **Campo de "ejecutar en T"** en los comandos (TODO #2 de `multinode.md`): el jitter de
  WiFi (10-100 ms) deja de importar porque cada nodo dispara con su timer local. Es el
  cambio de protocolo más chico con más rendimiento.
- **El colector es el secuenciador**: asigna franja a cada par (nodo, parlante). 480
  parlantes × 1 s = 8 minutos de barrido completo.
- **Códigos ortogonales** (TODO #5) para medir muchos a la vez y bajarlo a ~30 s. Vale
  la pena recién cuando el barrido de 8 min moleste.

---

## 9. Veredicto

**Los 15 sistemas son viables y el pronóstico mejoró respecto de julio**, pero el
objetivo hay que precisarlo:

- ✅ **Un foco a la vez**: de cabeza (±86 mm a 1 kHz) a puño (±43 mm a 2 kHz), con
  contraste de 12-25 dB. Sólido.
- ⚠️ **Ocho focos simultáneos**: sólo en **1,5-2,1 kHz**. Debajo de 1 kHz se funden.
- ✅ **Ocho focos multiplexados en el tiempo**: contraste completo, sin hardware nuevo.
- ✅ **Nivel**: sobra.
- 🔴 **La sala**: es el factor menos controlado y probablemente el que más pese.

**La decisión de comprar 12 sistemas más no debería tomarse antes de la prueba D** (§6):
mide con 3 grupos la única variable que gobierna el contraste multi-foco, y cuesta una
tarde.

Y hay una pregunta previa a todo: **¿los ocho focos tienen que sonar simultáneos, o
pueden alternarse?** Si pueden alternarse, la limitación central de este informe
desaparece y 15 nodos alcanzan con holgura.

---

## 10. La tira lineal — reconsideración a pedido de Fernando (2026-08-27)

> *"¿Y si el instrumento fuera una tira de 32 parlantes lo más cerca posible, 6 cm? Perder
> la poca focalidad que había en el eje vertical pero mejorar el plano horizontal. Es una
> instalación artística: si en la transición de foco a foco se escucha algo más difuso
> —incluso podríamos introducir ruido para competir con el campo difuso— no es grave."*

**La propuesta es mejor de lo que parece, y por una razón que no es obvia.**

### 10.1 El salto no es el paso de 80 a 60 mm — es dejar de repartir en dos ejes

| | apertura | techo de aliasing |
|---|---|---|
| grilla 8×4, paso 80 mm | 0,600 m | 2144 Hz |
| **tira 32 × 60 mm** | **1,860 m** | **2858 Hz** |

La grilla gasta 32 parlantes en dos ejes; la tira los pone todos en uno. **Son 3,1× de
apertura y 1,33× de techo, a la vez.** El foco se achica 3,1× a *todas* las frecuencias:

| f | grilla @3 m | tira @3 m |
|---|---|---|
| 500 Hz | ±1715 mm | **±553 mm** |
| 1 kHz | ±858 mm | **±277 mm** |
| 2 kHz | ±429 mm | **±138 mm** |

### 10.2 Y arregla el problema de la §3

La supresión propia de un módulo va como `10·log10(2L/λ)`. Al triplicar L, la tira suprime
~5 dB más que la grilla en toda la banda. Con 15 módulos y **8 focos simultáneos**:

| f | grilla | **tira** | |
|---|---|---|---|
| 500 Hz | +5,7 dB | **+10,7 dB** | usable |
| 1 kHz | +8,7 dB | **+13,7 dB** | usable |
| 2 kHz | +11,8 dB | **+16,7 dB** | usable |
| 2,8 kHz | aliaseado | **+18,1 dB** | usable |

> **La banda usable para 8 focos pasa de media octava (1,5-2,1 kHz) a dos octavas y media
> (500 Hz - 2,9 kHz).** Es exactamente el rango donde viven una voz y un instrumento. La
> §3 decía que 8 focos simultáneos eran marginales; **con la tira dejan de serlo.**

### 10.3 🔴 Y ya que lo difuso no molesta: espaciado APERIÓDICO

Ésta es la que más rinde dada la prioridad artística declarada. Simulado sobre el patrón
real de 32 elementos apuntando 30° fuera de eje:

| f | uniforme 60 mm | aperiódico, mismo largo | aperiódico sobre 3 m |
|---|---|---|---|
| 1 kHz | −13,2 dB · 5,4° | −13,0 dB · 5,4° | −8,7 dB · **3,9°** |
| 2,8 kHz | −13,2 dB · 1,9° | −11,8 dB · 1,9° | −8,0 dB · **1,4°** |
| 4 kHz | **0,0 dB** ⚠️ · 1,3° | **−6,4 dB** · 1,3° | −8,0 dB · **1,0°** |
| 6 kHz | **0,0 dB** ⚠️ · 0,9° | **−6,4 dB** · 0,9° | −7,9 dB · **0,7°** |

Con paso uniforme, arriba de 2858 Hz el lóbulo lateral **salta a 0 dB**: una copia del
foco, a nivel completo, en otro punto de la sala. Ése es el techo duro.

Con **espaciado aperiódico la copia no existe**: se disuelve en un piso parejo de −6 a
−8 dB. **El techo de aliasing desaparece.** El precio es exactamente lo que Fernando dijo
que no le molesta: un fondo difuso un poco más alto y sin estructura.

Y la tercera columna abre algo más: **el espaciado aperiódico desacopla "qué tan cerrado"
(la apertura) de "qué tan limpio" (la cantidad de elementos).** Con 32 elementos repartidos
irregularmente sobre 3 m el foco es más angosto a todas las frecuencias, sin ninguna copia
y con el piso plano. **La longitud de la tira pasa a ser una decisión de escenografía, no
de acústica.**

### 10.4 La cama de ruido: funciona, y sólo con la tira

La propuesta de meter ruido para competir con el campo difuso es una técnica de
enmascaramiento legítima. La condición es que el ruido tape la fuga sin tapar el foco:

    fuga + 3 dB  <  ruido  <  foco − 6 dB

| configuración | contraste | ventana de ruido | |
|---|---|---|---|
| grilla 8×4 @1 kHz | +8,7 dB | **cerrada** | el ruido que tapa la fuga tapa el foco |
| tira @500 Hz | +10,7 dB | −7,7 … −6,0 dB | funciona, 1,7 dB de margen |
| **tira @1 kHz** | +13,7 dB | **−10,7 … −6,0 dB** | funciona, 4,7 dB |
| **tira @2 kHz** | +16,7 dB | **−13,7 … −6,0 dB** | funciona, 7,7 dB |

**Las dos intuiciones se necesitan mutuamente**: con la grilla la ventana está cerrada.
La tira la abre. Nivel de partida: **una cama difusa unos 8 dB por debajo del foco.**

### 10.5 Lo que se pierde, y lo que hay que hacer al respecto

1. **Todo el enfoque vertical.** El foco deja de ser un punto y pasa a ser una **línea
   vertical**. Para oyentes a altura de oreja da igual — y hasta ayuda, porque funciona
   parado y sentado. **No es una pérdida real para esta obra.**
2. **Más energía al piso y al techo** → más reverberación, que es el enemigo declarado de
   la §4. **Sube la prioridad del tratamiento del techo.**
3. **⚠️ Los 15 módulos NO pueden ser todos horizontales y paralelos.** Una tira enfoca
   sólo en el plano que la contiene. Si las quince son horizontales, el conjunto tampoco
   tiene resolución vertical. **Variar las orientaciones** —algunas horizontales, algunas
   inclinadas, algunas verticales— hace que la **apertura del conjunto** cubra las dos
   dimensiones aunque cada tira cubra una sola. Es la recomendación más importante que
   sale de este cambio.
4. **⚠️ El parlante tiene que entrar en 60 mm.** Eso es un driver de ≤55 mm (2"), que cae
   rápido debajo de 200-300 Hz. Para "reconocer un instrumento" eso cuesta cuerpo. Hay que
   verificar contra los parlantes reciclados antes de comprometerse — **puede ser el
   limitante real de toda la propuesta**, y es lo primero que hay que medir.

### 10.6 Lo que la psicoacústica regala, y que ninguna de estas cuentas incluye

Las tablas de arriba son físicas. La percepción es más generosa, justo en el caso más
difícil:

- **Liberación espacial del enmascaramiento.** Con voces que compiten —tu caso de las
  poesías simultáneas— la separación espacial da **6-12 dB de mejora efectiva** de
  inteligibilidad. Un contraste físico de +13 dB se comporta perceptualmente como +20.
  **El caso de uso más exigente es el que más ayuda recibe.**
- **Precedencia.** Si el sonido enfocado llega primero y el difuso después, el oyente
  localiza en el foco aunque las energías sean parecidas.
- **El material define qué tan localizado se lee.** Lo tonal (instrumentos) enfoca
  nítido; la voz, intermedio; el ruido enfoca mal y se vuelve ambiente. **Eso no es una
  limitación: es una paleta.** Lo tonal se percibe puntual, lo ruidoso envolvente — y
  "sonidos experimentales que incomoden" probablemente quieran ser envolventes.

### 10.7 Veredicto

**La tira es mejor que la grilla para esta obra, en todos los ejes que importan.** Y
combinada con espaciado aperiódico, elimina el único techo duro que tenía el sistema.

Orden sugerido de decisión:
1. **Verificar que un driver entre en 60 mm de paso** con suficiente respuesta en medios.
   Es la única condición que puede voltear todo lo demás.
2. **Aperiódico desde el principio.** No cuesta nada en fabricación —es otro archivo de
   CNC— y compra una octava de banda.
3. **Decidir el largo por escenografía**, no por acústica. Más largo = foco más cerrado,
   mismo piso.
4. **Variar las orientaciones** entre módulos.
5. La prueba D de la §6 sigue siendo la que decide la compra, ahora con tiras.
