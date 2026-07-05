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

> **ARQUITECTURA REAL (aclarada por Fernando, 2026-07-05):** la señal de audio se
> distribuye **analógica** por el daisy-chain (J9, passthrough pasivo); cada nodo la
> digitaliza, beamformea y reproduce **con su propio cristal**. NO se manda MCLK por
> el cable (24.576 MHz por daisy-chain = pesadilla de integridad de señal); JP2 existe
> solo como salvavidas experimental de banco.

### Por qué la distribución analógica se auto-sincroniza

El modelo mental correcto: **cada nodo es una línea de retardo analógica**. Analógico
entra, analógico sale, retardado τ = N/fs_local (N = latencia del pipeline en muestras).
El reloj local solo importa durante los ~20 ms que la señal vive adentro del nodo — el
error de fase entre nodos **no se acumula** con el tiempo (a diferencia del contenido
generado internamente):

    Δτ entre nodos = latencia × Δppm = 20 ms × 40 ppm ≈ 0,8 µs ≈ 0,27 mm

Dos órdenes de magnitud por debajo del tamaño del foco. La fase del "grid" de muestreo
de cada ADC (que sí precede lentamente entre nodos) **no aparece en la salida**:
muestrear y reconstruir con el mismo reloj reproduce x(t−τ) exacto para señal limitada
en banda. La deriva térmica del cristal mueve ese sub-µs, no lo integra.

**Conclusión: para obras alimentadas por la señal analógica del daisy-chain — el caso
de uso real; el pipeline TX ya lee de `beam_line`, que viene del ADC — NO hace falta
ningún cable de clock.**

### Los dos "peros" reales (ninguno pide cable de clock)

1. **La latencia por nodo debe ser constante y conocida.** El offset RX-DMA↔TX-DMA se
   congela al arrancar y puede diferir entre nodos (hasta ~medio buffer, ms). Es un
   offset **constante** (no deriva) → se absorbe como delay por nodo:
   - por diseño: arranque determinístico del TX enganchado al callback del RX, o
   - por medición: AUTOFOCUS / chirp de red al encender (ya diseñado).
2. **El contenido generado internamente sí deriva.** Tonos de test, walker, chirps:
   no pasan por el ADC → corren con cristal libre → entre nodos derivan ~1 ms cada
   20-50 s. Los modos de test multi-nodo se alinean solo por envolvente; las obras
   van por la vía analógica. (AUTOFOCUS normal es por-nodo — mic y parlante en el
   mismo reloj — sin problema.)

   *Nota para calibración cruzada* (mic en nodo A midiendo parlantes del nodo B): la
   deriva durante el scan (50 ppm × 30 s ≈ 1,5 ms) contamina la comparación entre
   canales tempranos y tardíos → scans cortos o re-referenciar intercalando un canal
   fijo.

### Si algún día hiciera falta un clock físico (no ahora)

Solo si el proyecto migra a **distribución digital** del audio (más canales
independientes por nodo que los 8 analógicos). Para ese caso la opción correcta ya
está documentada en DS11_PLANNING (interconexión): **TDM re-driven sobre LVDS** por
Cat5e STP — MCLK+BICK+LRCK+DATA en los 4 pares, regenerado por cada caja (repeater),
terminación 100 Ω, drivers tipo DS90LV047/048, conector RJ45, mismo tendido. El clock
viaja embebido en el link y los SAI esclavos corren de reloj externo (BCK/FS
incluidos — resuelve de paso la objeción MCLK-no-es-BCK/FS de la revisión anterior).
Nunca 24.576 MHz crudo single-ended por el chain.

### JP2, entonces

Queda como lo que Fernando lo diseñó: un **hedge experimental** — probar clock común
entre 2 placas en banco, sin promesa de señal limpia en cable largo. No es parte de la
arquitectura de la instalación. (Verificado en netlist: puente simple sin buffer,
pin1→trunk `SAI1_MCLK`, pin2→J5.7.)

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
| A | 2 nodos | Misma señal analógica a ambos → mic + correlación cruzada: verificar Δτ inter-nodo estable (sub-µs) durante 30+ min y a través de reboots; medir el offset de latencia por nodo; **primer foco de 64 parlantes alimentado por señal externa** |
| B | 2-4 nodos + 1 mic por nodo | Auto-survey acústico two-way → geometría automática; comparar contra cinta métrica |
| C | 10-15 nodos | Director OSC + partituras espaciales; estrés de red WiFi (si satura → los ESP32 a un AP dedicado o Ethernet-over-cat5 del daisy chain) |

### TODOs de firmware que habilita esto
1. `CMD_GET_CAL_RESULT` — exportar delays/signos/calidad del AUTOFOCUS (hoy solo serial).
2. Timestamps de emisión/captura en cuentas de muestra (para two-way ranging).
3. Chirps con códigos ortogonales (medir 32 canales en 1 s).
4. mDNS + `device_id` único por nodo (ya existe `DEVICE_ID`).
5. Arranque determinístico del pipeline (latencia RX→TX fija en muestras) o chirp de auto-alineación de latencia al boot.
6. Sync de arranque: comando "arm" + trigger común (red o GPIO).

---

## 5. Qué NO hace falta resolver ahora

- Streaming de audio multicanal por red (las obras v1 usan el generador interno +
  delays; el contenido por red es otra etapa).
- Cable de clock entre nodos: la distribución analógica lo hace innecesario para el caso de uso real (§2).
- Posicionamiento del público (otra capa, va por el lado de la app / UWB / visión).

Ver también: [`app_ios.md`](app_ios.md) (roadmap de la app), `NodesStore.swift`
(transformación y fan-out), [`ds11_upgrades.md`](ds11_upgrades.md) (JP2/MCLK).
