# DS12 — Revisión de alimentación, ronda 2: plan para Opus

> Escrito el 2026-10-02. Continúa [DS12_REVISION_ALIMENTACION_PLAN.md](DS12_REVISION_ALIMENTACION_PLAN.md)
> y [DS12_REVISION_ALIMENTACION.md](DS12_REVISION_ALIMENTACION.md) (resultados de la ronda 1: S1, S2, S4, N1–N6).
> Lo lee Fernando, que es arquitecto: cada hallazgo explica el **mecanismo**, no sólo el resultado.


> **Estado al 2026-10-07: TERMINADA.** Los bloques A, B, C y E están hechos, con dos refutaciones a ciegas;
> los dos datos plantados se detectaron. Del bloque D, el D.1 está aplicado; los D.2–D.4, propuestos.
> Resultados: [DS12_REVISION_ALIMENTACION.md](DS12_REVISION_ALIMENTACION.md) §9. Informes y scripts:
> [`revision_alimentacion/ronda2/`](revision_alimentacion/ronda2/). Medidas de banco:
> [`revision_alimentacion/medidas_ds11.md`](revision_alimentacion/medidas_ds11.md).
> Lo que queda abierto se cierra midiendo (§9.4 del informe).

## 0. Por qué esta ronda, y en qué orden

Fernando todavía **no dibujó los cambios** ni **midió nada en una DS11**. Entonces el trabajo de papel
vale más ahora que después, por dos razones:

1. Hay cuatro cosas que **cambian lo que va a dibujar** y conviene saberlas antes (Bloque A).
2. Los arreglos que propuso la ronda 1 **no pasaron por refutación**: sólo se refutó el *mecanismo* de
   cada problema, no la *solución*. Dibujar una solución sin refutar es como pedir con un error
   sin revisar.

Orden: **A (lo que se dibuja) → B (verificar lo que el artifact heredó) → C (lo que queda del plan)
→ D (documentación) → E (preparar la sesión de banco)**. Si el presupuesto no alcanza, A y B no se saltean.

**Precondición verificada el 2026-10-02:** el esquemático **no cambió** respecto de ayer. Regeneré el
netlist con `tools/sch2net.py` y `tools/netlist_diff.py` dio 0 componentes y 0 conexiones distintas,
aunque los `.kicad_sch` tienen fecha de hoy 08:45 (la pisó una sincronización, no una edición).
`DS.net` es de ayer 15:29:40 y sigue vigente. **Antes de empezar, repetir esa comprobación**: si
Fernando dibujó algo mientras tanto, regenerar el digest (`tools/netlist_digest.py hardware/DS/DS.net .ds12_audit`)
y mirar qué cambió.

## 1. Reglas (las de ayer, sin cambios)

- **Sólo esquemático.** No abrir `DS.kicad_pcb`, no correr `check_pcb.sh`, no opinar de layout. Lo que dependa
  del layout va a una lista aparte, sin desarrollar.
- **Las entradas del ADC sin polarización** siguen siendo de Fernando (ver C4: sólo se prepara material).
- **Nunca contactar las placas** (192.168.100.x) ni encender nada. Encender los amplificadores es una
  decisión física de una persona.
- **No commitear ni tocar el esquemático ni el `.net`.** Los diffs de documentos se proponen, no se aplican, hasta que
  Fernando diga cuáles.
- Todo hallazgo lleva **cómo se verificó**: comando sobre el digest, documento + página + frase, o script reproducible.
  Los scripts nuevos llevan **autoprueba con respuesta conocida y una prueba negativa** (regla del repo).
- Stock y disponibilidad no se analizan: ya está comprado. Los arreglos prefieren piezas que **ya están en
  `jlcpcb/production_files/BOM-DS.csv`**.
- Los datasheets que falten se bajan a `hardware/DS/datasheets/`. La norma USB 2.0 completa quedó en
  `/private/tmp/claude-501/-Users-bergamifer-STM32CubeIDE-DS-Firmware/e765fbd8-4afd-49e7-ba8e-d9b5d943e369/scratchpad/ds/usb20/usb_20.pdf`
  (si ya no está, bajarla de usb.org; el zip es `usb_20_20250603.zip`).
- **Orquestación:** 3 analistas con alcances separados + refutación **a ciegas** (el refutador recibe la afirmación y la
  evidencia, no el razonamiento) + Opus de árbitro. Como control de calidad, **plantar un dato falso** en una de las
  evidencias y comprobar que el refutador lo detecta. La ronda 1 lo hizo y funcionó.
- Scripts existentes, para reusar y no reescribir: `hardware/DS/revision_alimentacion/`
  (`s1_reparto.py`, `s4_hotplug.py`, `s4f_verif.py`, `s4f_mitig.py`; los cuatro con autoprueba).

---

## 2. Bloque A — lo que cambia lo que Fernando va a dibujar

### A1 · ¿Cuántos `C45783` hacen falta en `+5V_AUX`? (decide el paso 1 del artifact)

El artifact dice «dos `C45783`, y tres si cada uno conserva menos de 11 µF a 5 V». Eso es una
**regla de pulgar mía, no un resultado**. Hacer:

1. **Identificar la pieza real.** `componentes.tsv` la describe como «Electrolytic» en C67/C69 y el nombre dice «X5R, 0805».
   Confirmar en la página de LCSC de `C45783` fabricante, dieléctrico, tensión nominal y si hay curva de
   capacidad contra tensión continua.
2. **Capacidad efectiva a 5 V** (y a 5,5 V, y a 25/60/85 °C). Con eso, rehacer `s4f_verif.py`: el pico de `+5V_AUX`
   para N = 1, 2, 3, 4 capacitores, con ESR/ESL del 0805, en el barrido de cable (0,3/0,7/1,5 µH × 0,08/0,15/0,30 Ω),
   VBUS 5,25 V y con el clamp de U9 en 0,5 Ω y en 2 Ω.
3. **Criterio de aceptación a proponer:** pico ≤ 5,8 V (margen de 0,2 V bajo los 6,0 V de operación del AP2125) en el peor caso.
   Si ningún N razonable lo logra, decirlo y **estudiar la alternativa**: un TVS de 5 V o un zener en `+5V_AUX`, o un
   diodo/limitación en D8 (pieza que ya esté en el BOM).
4. **Efectos colaterales de agregar capacidad**, a cuantificar con `s4_hotplug.py`: corriente y caída del hub en modo
   sólo-USB (ya hay 48 µF), y si la capacidad extra empeora N4 o S4.
5. **El mismo problema vale para J3 en el caso en que J3 quede con una resistencia de descarga** (ver A3): ahí C31 arranca
   descargado y J3 se vuelve tan vulnerable como J8. Resolverlo junto.

### A2 · Refutar el arreglo de N1 (la resistencia de 1 kΩ en `+5V_AUX`)

La ronda 1 refutó el *mecanismo* de la fuga de D23, no el arreglo. Hacer:

1. **Digitalizar la Fig. 2 del SS34** (MDD, `datasheets/SS34_MDD.pdf`, p.2) y armar la corriente de fuga contra temperatura
   a 15 V (D23) y a ~4,8 V (D2, D8, D11). Usar el máximo especificado (0,5 mA a 25 °C, 5 mA a 125 °C, p.1) como cota dura
   además de los típicos.
2. **Calcular el punto de equilibrio de `+5V_AUX`** con el AK5538 apagado (10–100 µA), el AP2125 en vacío (60–90 µA),
   las fugas de los cuatro diodos del OR y el drenaje candidato, de 25 a 85 °C. Barrer el drenaje: 1 kΩ, 2,2 kΩ, 4,7 kΩ, 10 kΩ.
3. **Elegir el valor** con el peor caso, y justificar que no es mayor al necesario: con 1 kΩ el drenaje toma ≈4,8 mA constantes
   de `+5VA` (23 mW), y en modo sólo-USB toma esos mismos 4,8 mA del USB. Decir si vale la pena un valor más alto.
4. **¿Hay un arreglo mejor que el drenaje?** Por ejemplo mantener al AK5538 encendido, o D23 con menos fuga (otro Schottky ya
   en el BOM), o sacar la dependencia. Compararlos por costo, componentes nuevos y robustez.

### A3 · N4 con la norma delante: ¿hace falta una resistencia de descarga en el VBUS de J3?

Hoy es una sospecha apoyada en que una fuga de ~1,5 µA de D2 deja el VBUS de J3 en ≈4,7 V. Falta:

1. **Leer los textos normativos** y citar frase y página: USB 2.0 §7.2.1 (back-voltage de un dispositivo autoalimentado; la
   prueba «Back Voltage» del *Electrical Compliance Test Specification*, ya descargado) y USB Type-C §4.5.2.2.8.2 (vSafe0V
   antes de Attached.SRC). **Distinguir** qué exige la norma de un *sumidero* con VBUS propia y qué exige de la *fuente*.
2. **Estimar qué tensión aparece realmente** en un VBUS abierto: la fuga de D2 contra lo único que lo carga (U6 USBLC6, C31,
   y la resistencia de detección de la fuente). Con 1,5 µA el nodo sube hasta que la fuga cae, y esa tensión depende de lo que haya del otro lado.
3. **Evaluar el arreglo:** una resistencia de descarga de J3 a GND (candidata: 100 kΩ `C25741`, o la misma topología de J8,
   que ya tiene 162 kΩ en R161/R160). Calcular tensión en reposo, corriente que drena de `+5V_AUX`, y si interfiere con S1.
4. **Qué mide Fernando** (paso 4 del artifact) y **qué hacer con cada resultado**: lectura de VBUS de J3 con el 20 V puesto y sin
   cable, y si la Mac enumera el ESP32. Dejar una tabla «lectura → decisión».

### A4 · PE15 como jumper de identidad (decide un componente que hay que pedir ahora)

El artifact propone una resistencia de 0 Ω de PE15 a GND en la DS12 y PE15 al aire en la DS11. Hoy `docs/pin_map.md:122`
dice «PE15 sin usar, GPIO_ANALOG». Verificar:

1. **Funciones alternativas de PE15 en el STM32H743VI** (hoja DS12110 Rev 10, tabla de funciones alternativas) y que ninguna
   esté en uso en esta placa. Verificar también `firmware/DS-Firmware.ioc` y `pin_definitions.h`.
2. **Comportamiento durante y después del reset**: PE15 arranca en analógico (DS12110 p.32), así que el firmware tiene que
   configurarlo como entrada con pull-up antes de leerlo. Confirmar que un pull-up interno (≈40 kΩ típico) contra un
   0 Ω a GND lee un 0 limpio, y que al aire lee 1 limpio incluso con el cable de J7 cerca.
3. **¿Existe el 0 Ω en el BOM?** Si no, ¿una resistencia de otro valor (p. ej. 10 kΩ `C25744`) sirve igual? ¿O un puente de soldadura?
4. **¿Hay un pin mejor** entre los libres, lejos de relojes, con función alternativa inofensiva? Proponer uno sólo si PE15 tiene un problema.

### A5 · Cubrir lo que nadie miró: el crítico de completitud

La auditoría del 11-sep dejó anotado que «el crítico de completitud nunca corrió». Correrlo ahora sobre la alimentación:

1. A partir del digest, listar **todos los pines de alimentación de todos los chips** (pines conectados a `+5VA`, `+5VA_DAC`,
   `+3V3`, `+3.3VA`, `+5V_AUX`, `+VIN_LOCAL`, `/DS_Power/IN` y GND de cada IC) y armar una **matriz de cobertura**: cuáles se
   verificaron contra datasheet (ronda 1 y 29-sep) y cuáles no.
2. Lo que quede sin verificar entra al Bloque C.
3. Revisar también los **componentes pasivos de potencia** que nadie miró: C60 (22 µF/50 V Al, su tensión en el nodo `IN`),
   C318/C323 (Cff), C317/C61 (BST), el bypass de VIN de cada buck y su tensión nominal contra la tensión del nodo (el error
   de C315/C320 apareció así: **repetir esa revisión de tensión nominal contra tensión real en todos los capacitores de los nodos de 15–21 V, 5 V y 3,3 V**).

---

## 3. Bloque B — el artifact heredó afirmaciones que no se verificaron

El artifact publicado ayer conserva texto de sesiones anteriores que **no volví a verificar** y que dice cosas
como cierto. Antes de que Fernando se apoye en ellas, cada una pasa por el mismo proceso (cita o cuenta + refutación):

| afirmación heredada | dónde está | qué verificar |
|---|---|---|
| El buck A carga 1,15 mF en 4 ms con ~1,5 A, «lejos de su límite de 4–6 A, no entra en protección» | «Lo que descarté» | tiempo de soft-start del AP63356 (DS41949), límite de corriente pico, comportamiento de hiccup, capacidad de salida máxima si la hay |
| El lazo del AP63356 cruza cerca de 1,4 kHz con 1,15 mF de salida | «Primera prueba de lo nuevo» | de dónde sale el número; si la hoja no da la ganancia del lazo con compensación interna, el hallazgo pasa a **ABIERTO** |
| UVLO del buck A: enciende entre 13,9 y 14,7 V, apaga entre 10,4 y 11,9 V; `+5VA` 4,95–5,23 V; `+3V3` 3,20–3,36 V | «Las decisiones, con los números» | recalcular con script y peor caso (R ±1 %, VFB ±1 %, corrientes I1/I2 del pin EN). Reproducir primero los nominales 14,3/11,1 V, 5,088 V y 3,280 V |
| MAX809S «dispara a 3,00 V como máximo» | ídem | bajar la hoja del MAX809S de UMW (`C347371`) y leer umbral mín/típ/máx |
| Q1 y su zener: compuerta en 10,5 V como máximo, 20 V en inversa contra 30 V | ídem | AO3401A ya está en `datasheets/` (VGS ±12 V, VDS −30 V); BZT52C10 hay que bajarla |
| TVS «no conduce con 21 V; clampea a 32,4 V» | ídem | SMBJ20A: VRWM, VBR mín, fuga, VC a Ipp |
| Inductores «saturan arriba del límite de corriente» | ídem | L1/L2: Isat contra IPEAK_LIMIT de cada chip (DS42002 p.4: 4,1/4,5/4,9 A para el AP63301; DS41949 para el AP63356) |
| «El AP63301 arranca con ≤3,7 V» | ya verificado el 1-oct (DS42002 p.4), **sin** la histéresis | además: umbral de caída (3,7 − 0,44 V) frente a un pico de TX del ESP32 con `VIN_B` en 3,84 V |
| «No usar el DFU de fábrica la primera vez» y «PA9/PA10 libres para el bootloader» | «Primer encendido» | es consejo de la sesión del 29-sep; contrastar con AN2606 (bootloader del H743) |

Resultado esperado por fila: **CONFIRMADO / REFUTADO / ABIERTO**, y el texto del artifact corregido (propuesto, no aplicado).

---

## 4. Bloque C — lo que queda del plan original

Orden por probabilidad de importar. Mis impresiones iniciales (🔎 **sin verificar**, sirven de semilla):

| # | tema | impresión previa | verificar |
|---|---|---|---|
| **N2** | la fuga de D8 «hace creer» al STM32 que hay un host (PA5) | probablemente 🟢 inofensiva: con el STM32 sin USB habilitado, una falsa detección no hace nada. El umbral es I ≥ 23 µA, porque PA5 = I × 100 kΩ y VIH = 2,31 V | confirmar el umbral con la hoja del H743; revisar si hay pull interno en PA5; **escribir qué debe hacer el firmware con PA5** |
| **N3** | el pull-up de D+ inyecta corriente al VBUS de una PC apagada | 🟡 plausible; depende de la hoja de la pata USB del H743 y de si el pull-up es interno | leer RM0433 / DS12110 sobre D+ y VBUS; norma USB 2.0 §7.2.1 |
| **N6** | reflashear entre 15 y 75 s cuenta como intento fallido del autoarranque | firmware, seguro | proponer el cambio mínimo |
| **S3** | estabilidad y arranque del AP63356 con 1,2 mF | ver Bloque B | |
| **S6** | FB69 (ferrite 0805, 1,5 A, 220 mΩ) con hasta 1 A de expansión | puede perder impedancia con el sesgo de CC | bajar la hoja de `C21286` (curva de impedancia contra corriente) |
| **S7** | C67 (22 µF) a la salida del AP2125 | probablemente dentro de la ventana de estabilidad, pero confirmar el rango de capacidad y ESR | DS38144 |
| **S8** | MAX809S contra el mínimo de 3,0 V del ESP32 | 🟢 casi seguro inofensivo: el MAX809 sólo resetea al STM32, el ESP32 tiene su propio detector | confirmar con el netlist (¿a qué pines va `NRST`?) y DS del ESP32-C3 |
| **S9** | PG de IC4 con VIN en 0 | 🟢 DS41949 p.5 da VPG −0,3 a +6 V **sin referirlo a VIN**, así que 3,3 V entra | buscar si hay alguna cláusula sobre el pin con el chip apagado |
| **S10** | la salida `+5VA` (1,1 mF) alimenta `VIN_LOCAL` por el diodo de cuerpo del lado alto de IC4 cuando cae la entrada | plausible; el efecto sería un «hold-up» de `+3V3` | cuenta de tiempos; límite de corriente inversa del diodo de cuerpo |
| **S11** | ruido hacia el AVDD del ADC por `+5VA → FB69 → D19 → +5V_AUX → U10 → +3.3VA` | **el único que toca la calidad de audio**; hoy sin cuantificar | PSRR del AP2125 a 200–500 kHz (digitalizar su gráfico), rizado de `+5VA` esperado de los PAM y del buck A, y la sensibilidad de AVDD/VREFH del AK5538 (la hoja da PSRR sólo a 1 kHz: 60 dB) |
| **S12** | un ladrillo de más de 20 V en vacío | 🟢 el artifact ya lo trata como «F1 abre» | VRWM = 20 V del SMBJ20A contra la fuga a 20–21 V |

Y lo que salga de la matriz de cobertura (A5).

### C4 · Entradas del ADC (sólo preparar material; la decisión es de Fernando)

Sin tocar la decisión: leer la sección de entradas analógicas del AK5538 (`datasheets/AK5538VN.pdf`) y dejar escrito, con
citas de página, **si el chip fija la continua de las patas internamente** (la «resistencia de entrada de 10,4 kΩ», ¿va a una
referencia interna o a masa?). Si va a una referencia, la medida de la DS11 sólo confirma un valor que ya se puede predecir.
Dejar la predicción y su incertidumbre, así que cuando Fernando mida pueda compararla.

---

## 5. Bloque D — documentación

**Sí hace falta actualizar, y lo que está desactualizado hoy es esto.** Todo se propone como diff y se aplica **cuando Fernando
diga cuáles**:

1. **Errores demostrados ayer** (hechos verificados contra el netlist y las hojas de datos; no dependen de ninguna decisión):
   - `DS12_ALIMENTACION.md` §5, tabla de modos, renglones 2 y 3: «D2 en reverso, la PC no entrega nada» (S1).
   - `DS12_ALIMENTACION.md` §6.1.a: «en la DS11 no pasaba» (S2).
   - `DS12_ALIMENTACION.md` §6.1.a y §12: R162/R163 y C327 contra el netlist, que dice R163 a `+5VA`, R162 a GND y C312 (N5).
   - `DS12_USB_STM32.md` línea 186: «la PC entrega poco o nada» (S1).
   - `DS12_ALIMENTACION.md` §11.3: todavía pide `C1525` (16 V) para el bypass de VIN; el esquemático ya usa `C14663`.
   - `docs/presupuesto_potencia.md`: sigue recomendando 12 V en banco, que con la DS12 deja a los amplificadores apagados.
2. **Pendientes de las decisiones de Fernando** (se redactan, no se aplican): capacitores de `+5V_AUX` (A1), drenaje (A2),
   resistencia de J3 (A3), jumper (A4), D24/D25 y TP9.
3. **`DS12_CHECKLIST.md`**: agregar los ítems nuevos y tildar lo que ya está dibujado (C315/C320 con `C14663`, FL1 con `C97855`).
4. **Dónde está cada cosa:** el `hardware/DS/README.md` y la tabla del `CLAUDE.md` tienen que apuntar a `DS12_REVISION_ALIMENTACION*.md`
   y a `revision_alimentacion/`, y `tools/README.md` o el README de esa carpeta tiene que explicar qué hace cada script y cómo correrlo.
5. **Qué entra al repo:** el informe, los dos planes, los cuatro scripts. Las hojas de datos (11 MB, siete archivos nuevos)
   están sin commitear; verificar `git check-ignore` y decidir con Fernando si van al repo o a `.gitignore`.
   **Si el commit incluye el PCB, antes correr `tools/check_pcb.sh`.** No commitear nada sin que Fernando lo pida.
6. **Memoria del proyecto:** actualizar `ds12-backlog.md` con los veredictos de esta ronda.

## 6. Bloque E — preparar la sesión de banco con la DS11

Hoy el paso 4 del artifact tiene las medidas pero no un lugar donde anotar los resultados ni qué decidir con cada uno.
Dejar listo para que Fernando se siente a medir:

1. **Una tabla de resultados en blanco**, en `hardware/DS/revision_alimentacion/medidas_ds11.md`: medida, instrumento,
   valor esperado, valor leído, decisión que dispara. Cubre: continua de las patas del ADC, VBUS de J3 con el 20 V puesto,
   óhmetro de SD (1,033 kΩ), tester en modo diodo (con la corrección del falso positivo), medidor USB de J3 (0–45 mA), y
   TP8 con el STM32 en blanco (N1).
2. **Un árbol de decisión por medida**: «si da X, se hace Y», con el componente y el LCSC.
3. **Seguridad:** ninguna medida enciende los amplificadores ni requiere acceso HTTP a la placa. Dejarlo escrito arriba de la tabla.
4. **Cuando Fernando traiga números**, Opus los ingiere: corrige el informe, el artifact y la memoria, y re-corre los scripts
   afectados con los valores reales (p. ej. `s4f_verif.py` con la capacidad medida).

---

## 7. Entregables y criterio de cierre

1. `hardware/DS/DS12_REVISION_ALIMENTACION.md` actualizado con una sección «Ronda 2»: veredictos por ítem
   (CONFIRMADO / REFUTADO / ABIERTO), cada uno con su evidencia.
2. **Una recomendación cerrada para cada cosa que Fernando va a dibujar** (A1–A4), con valores y LCSC que ya estén en el BOM,
   o la indicación de que hace falta una pieza nueva.
3. Los diffs de documentación del Bloque D, sin aplicar.
4. `medidas_ds11.md` (Bloque E).
5. El artifact «Cambios para DS12» (`claude.ai/artifact/CRNWSgs1K8WE6eEpN3Yexs`) actualizado al final, con lo que cambió
   (lo heredado y refutado sale o se corrige).

**Cuándo parar:** cuando A y B estén cerrados. C, D y E se hacen mientras alcance el presupuesto, en ese orden.
Lo que no se pueda cerrar con las hojas de datos queda **ABIERTO** con la medida que lo resolvería; nunca «confirmado de memoria».
