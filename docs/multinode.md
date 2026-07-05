# Multi-nodo: coordinar 10-15 muros (~500 parlantes) sin conocer su disposición

> Escrito 2026-07-05. Responde: *"¿cómo nos preparamos para cuando el setup incluya 10-15
> de estos sistemas? ¿cómo coordinar todos los parlantes si no sabemos la disposición
> espacial?"* — La respuesta tiene tres capas independientes: **geometría** (dónde está
> cada parlante), **reloj** (cuándo dispara cada uno) y **control** (quién decide qué).

---

## 0. El estado de partida

Cada nodo DS10/DS11: 32 parlantes con geometría interna **conocida y rígida** (el muro),
STM32 que calcula delays muestra-exactos **dentro del nodo**, ESP32 con HTTP en la LAN.
Lo desconocido al montar: la **pose de cada muro** (posición + orientación en la sala) y
la **relación entre relojes** de los nodos.

La app iOS ya implementa la base: registro de nodos con pose (x, z, yaw), transformación
del foco global al marco local de cada nodo, y envío fan-out paralelo (`NodesStore.swift`).
**Con eso, 15 nodos ya se coordinan hoy a nivel de comandos** — lo que falta resolver es
geometría fina y fase.

---

## 1. Capa GEOMETRÍA — ¿dónde está cada parlante?

### Nivel 0 (hoy, sin desarrollo): pose manual + foco medido
- Cinta métrica: posición y yaw de cada muro → editor de nodos de la app.
- El error de ~5-10 cm de medir a mano **se absorbe con AUTOFOCUS**: para los puntos
  focales importantes de una obra, el foco se mide con micrófono, no se calcula.
- Suficiente para: instalación estática con focos ensayados.

### Nivel 1: survey con el iPhone (ARKit)
- El iPhone se localiza solo (~cm) caminando la sala; tocás la esquina de cada muro
  (o apuntás y el LiDAR mide) → poses de los 15 muros en un marco común, en minutos.
- Requiere: app en el teléfono físico (v2a del roadmap).

### Nivel 2: auto-survey acústico — el instrumento se releva a sí mismo
La joya, y es factible con el hardware existente: **cada nodo tiene 8 entradas ADC**.
Con 1 micrófono (barato, de medición) por nodo:

1. El nodo *j* emite un chirp por su parlante *k* (el walker ya hace esto).
2. **Todos** los nodos graban y correlacionan (el AUTOFOCUS ya hace esto).
3. Matriz de TOF entre (nodo j, parlante k) → (nodo i, mic i).
4. Multilateración / MDS (multidimensional scaling) → posiciones de todos los muros
   **a escala real** (la velocidad del sonido da la escala), salvo rotación/traslación
   global — que se fija midiendo 2-3 puntos a mano o con el iPhone.
5. Con 32 fuentes por muro, la **orientación** de cada muro también sale del ajuste.

**El detalle del reloj**: el TOF medido entre nodos incluye el offset de reloj del
receptor. Solución clásica (BeepBeep, Microsoft Research): medición **two-way**
(*j* emite y *i* graba; después *i* emite y *j* graba) — al promediar ambas, el offset
se cancela exactamente. Precisión esperable: ~1 cm.

> Prerrequisito de firmware: timestamp de emisión/captura en cuentas de muestra del
> propio nodo + comando para reportar la correlación (`CMD_GET_CAL_RESULT` extendido).

---

## 2. Capa RELOJ — ¿cuándo dispara cada parlante?

El problema **más duro** y el que define qué obras son posibles:

- Cristales independientes → deriva típica 20-50 ppm → **1 ms cada 20-50 s**.
  Sin reloj común, la fase entre nodos se va a la deriva: un foco único formado por
  2+ muros se desarma en segundos.

### El hardware ya tiene el camino: JP2 — pero solo resuelve media historia

Verificado en el netlist (`DS.txt`): JP2 es un puente de soldadura simple (sin buffer,
sin selector de dirección) — pin 1 al trunk interno `SAI1_MCLK` (el mismo que alimenta
los 4 DACs vía RN1-4 y el ADC vía R131), pin 2 al conector J5 pin 7. Puentearlo permite
exportar/inyectar el MCLK de 24.576 MHz entre placas por el mismo tendido físico del
daisy-chain de 24 V.

**Corrección importante (2026-07-05) sobre lo que esto alcanza a resolver.** MCLK es
la referencia de sobremuestreo que llega al SCKI del DAC/ADC — pero **no** es la señal
que fija el instante de cada muestra: eso lo hace **BCK/FS** (el reloj de bit y el
frame sync del TDM), que cada STM32 sigue generando de forma independiente, con su
propio cristal/PLL3, aunque su JP2 esté puenteado. Compartir solo MCLK probablemente
**no alcanza** para lock de fase completo entre nodos — el drift de BCK/FS (ppm de
cada cristal) queda igual, un nivel más abajo. Esto es una hipótesis de ingeniería
razonada, no un hecho confirmado con el datasheet del PCM1690 en mano.

Lo que sí resolvería el lock completo:
1. **Compartir también BCK+FS** (no solo MCLK) y correr el SAI de los nodos esclavos
   en modo *slave* externo — el firmware ya sabe hacer esto *dentro de un mismo chip*
   (SAI1_B/SAI2_B como slaves de SAI1_A/SAI2_A, "arrancar slave antes que master");
   falta validar si se puede extender esa sincronización **entre chips** por cable.
2. **Disciplinar el PLL3 de cada esclavo** contra el MCLK entrante (clock recovery
   por software/hardware) — más trabajo de firmware, pero el esclavo termina generando
   su propio BCK/FS ya enclavado al mismo reloj físico que el master.

**Cómo se decide esto en la práctica**: es exactamente el experimento de la Fase A de
abajo. Con 2 nodos y JP2 puenteado, medir con un mic + correlación cruzada (mismo
método del AUTOFOCUS) si el desfasaje entre nodos se mantiene estable o sigue
derivando con el tiempo. Si deriva, confirma que hace falta compartir BCK/FS también.

Independientemente de cuál solución haga falta, **sigue existiendo** el problema de
alinear el **t=0** (que todos cuenten la muestra cero juntos). Dos opciones:
1. **Pulso de sync** por GPIO en el mismo cable (hardware trivial, agregar a DS12).
2. **Alineación acústica** (sin hardware nuevo): comando de red arranca todos
   "aproximadamente"; después el nodo A emite un chirp y el nodo B lo captura →
   el offset de arranque queda **medido** y se compensa como delay constante por
   nodo. Estable mientras el reloj compartido no se desconecte.

### Sin MCLK compartido (setup rápido / nodos lejanos)
- Coherencia **intra-nodo** perfecta (ya la hay) + alineación **inter-nodo** de
  envolvente (~1 ms) vía sync de red estilo NTP contra el director.
- Alcanza para: trayectorias entre muros, texturas espaciales, focos *por muro*.
- No alcanza para: un foco único sumando 2+ muros en fase.

**Regla de diseño para las obras**: foco fino = dentro de un muro; gestos grandes =
entre muros. Con JP2 encadenado, esa restricción desaparece.

---

## 3. Capa CONTROL — ¿quién decide?

- **Hoy (implementado)**: la app iOS hace fan-out directo — foco global → marco local
  de cada nodo → `POST /beam` en paralelo. HTTP liviano: 15 nodos × 1 request por gesto
  es nada. Escala hasta el taller completo sin más infraestructura.
- **Siguiente**: un **director** (laptop o RPi en la sala) que habla OSC/WebSocket con
  las herramientas musicales (Max, SC, Ableton) y reparte a los ESP32. La app pasa a
  hablar con el director. Necesario cuando haya: partituras con timing fino, varias
  fuentes de control simultáneas, u obras que corren solas.
- **Descubrimiento**: mDNS en el ESP32 (`domosonica-N.local`) → la app arma la lista
  de nodos sola. (TODO firmware, trivial con ESP-IDF.)

---

## 4. Plan concreto por fases (cuando lleguen las DS11)

| Fase | Con qué | Qué validar |
|---|---|---|
| A | 2 nodos | MCLK compartido por JP2 (¿integridad de señal en el cable? R de amortiguación), alineación t=0 acústica, **primer foco de 64 parlantes** |
| B | 2-4 nodos + 1 mic por nodo | Auto-survey acústico two-way → geometría automática; comparar contra cinta métrica |
| C | 10-15 nodos | Director OSC + partituras espaciales; estrés de red WiFi (si satura → los ESP32 a un AP dedicado o Ethernet-over-cat5 del daisy chain) |

### TODOs de firmware que habilita esto
1. `CMD_GET_CAL_RESULT` — exportar delays/signos/calidad del AUTOFOCUS (hoy solo serial).
2. Timestamps de emisión/captura en cuentas de muestra (para two-way ranging).
3. Chirps con códigos ortogonales (medir 32 canales en 1 s).
4. mDNS + `device_id` único por nodo (ya existe `DEVICE_ID`).
5. Selector master/slave de MCLK por comando (hoy JP2 es un jumper físico — ok para v1).
6. Sync de arranque: comando "arm" + trigger común (red o GPIO).

---

## 5. Qué NO hace falta resolver ahora

- Streaming de audio multicanal por red (las obras v1 usan el generador interno +
  delays; el contenido por red es otra etapa).
- Sub-muestra entre nodos vía software puro (con JP2 no hace falta; sin JP2 no se puede).
- Posicionamiento del público (otra capa, va por el lado de la app / UWB / visión).

Ver también: [`app_ios.md`](app_ios.md) (roadmap de la app), `NodesStore.swift`
(transformación y fan-out), [`ds11_upgrades.md`](ds11_upgrades.md) (JP2/MCLK).
