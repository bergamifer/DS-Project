# DS12 — Backlog de hardware (próxima versión / lote)

> Abierto 2026-07-08, con la **DS11 ya enviada a fabricar**. Acá se acumula todo lo que
> se detectó tarde o se decidió posponer, para no perderlo. Nada de esto bloqueó la DS11;
> son mejoras/verificaciones para la próxima vuelta. Ver [DS11_PLANNING.md](DS11_PLANNING.md).

---

> **Revisión cruzada 2026-08-17** (netlist + datasheets): apareció una tanda de
> hallazgos nuevos, varios de ellos P0. Están abajo como ítems 0.x, antes que el
> resto, porque **cambian decisiones de arquitectura** y conviene resolverlos antes
> de tocar nada más. Los que involucran alimentación están verificados contra
> `DS.txt` y el BOM de producción; el análisis completo, en `../../docs/presupuesto_potencia.md` §3bis.

## 🔴 Prioridad 0 — decidir antes que nada

### 0.1. Riel digital+analógico independiente del riel de amplificadores
Hoy `+3V3` y `+3,3VA` cuelgan de `+5VA` por **FB2 (bead genérico) + D9 (SS34)**, que
se comen ~0,5 V y dejan al AP1117-33 con ~250-350 mV sobre su dropout. **Esto explica
el brown-out de agosto mejor que la falta de bulk**: no es energía almacenada, es
margen de dropout en serie. Y de paso mete el riel del ADC aguas abajo de 32 amps
class-D. **Solución: buck propio para el riel digital**, desde la entrada.

#### ⚠️ 2026-09-02 — deja de ser teoría: medido en campo

Primera vez que una DS11 corrió **sin ningún USB conectado** (ST-Link y USB del
ESP32 fuera). Resultado: el STM32 se reinicia cada ~10 s en frío, y siempre al
encender los amplificadores. Reconectar el ST-Link lo hace desaparecer.

No es magia: **J7 alimenta la placa**. El pin 10 entra por D11 al `+5V`, o sea
**aguas abajo de FB2 y D9** — le da al AP1117-33 el margen de dropout que la
cadena propia de la placa no tiene. (El pin 1 va directo a `+3V3`, ítem 2b.)

**Consecuencia práctica: las cinco DS11 fabricadas no funcionan solas.** Todos los
bring-ups corrieron con esa muleta puesta sin que figurara como tal.

Firma para reconocerlo por red, sin instrumental: **el ESP32 sigue vivo mientras
el STM32 se reinicia** (uptime 500 s contra 10 s). El ESP32 aguanta hasta ~2,7 V
y el MAX809T corta a 3,08 V. Desde proto v14 `/status` publica `reset`: **POR =
riel**, PIN = NRST.

El amperímetro de la fuente **no lo ve**: promedia a la tensión de entrada
(0,25 A, normal) mientras el evento son centenas de mV durante µs, dos
conversiones aguas abajo. No sirve para descartar esto.

> **Mitigación para las placas que ya existen** — vale la pena probarla antes que
> DS12: puentear FB2 y D9 con hilo (⚠️ **sólo** con ST-Link y USB desconectados;
> D9 es lo que impide el backfeed). Si los reinicios desaparecen, no es sólo el
> diagnóstico cerrado: es el arreglo de las cinco DS11.

### 0.2. El TVS: con 19-20 V y un buck de 26 V no existe solución
El ítem 1 propone "TVS de standoff más bajo (SMBJ18/20A)". **No funciona**: lo que
importa es el **VC (clamping)**, que en la familia SMBJ es ~1,6× el VRWM.

| parte | VRWM | VC | ¿< 26 V? |
|---|---|---|---|
| SMBJ13A | 13 V | 21,5 V | sí |
| SMBJ18A | 18 V | 29,2 V | **no** |
| SMBJ20A | 20 V | 32,4 V | **no** |
| SMBJ24A (actual) | 24 V | 38,9 V | no |

Para clampear bajo 26 V hace falta VRWM ≤ 16 V, **por debajo del punto de operación**.
Es geométricamente imposible. Las dos salidas coherentes:

- **A**: sistema a 12 V + MP2315S + **SMBJ13A**. Sólo sirve para una placa sola (ver
  la corrección de daisy-chain en `presupuesto_potencia.md` §1).
- **B (recomendada)**: sistema a 19-24 V y **se elige el buck para que quepa sobre el
  clamp del TVS**, no al revés. Con el SMBJ24A ya en BOM hace falta un buck de
  **VIN abs max ≥ 42 V**. El TVS pasa de inútil a correcto sin cambiar de parte.

En cualquier caso hace falta **impedancia serie** entre el conector y el TVS (el
fusible del 0.4 sirve): un TVS colgado de una fuente de baja impedancia no coordina.

### 0.3. Los amplificadores están limitados térmicamente antes que el buck
El encapsulado es **MSOP-8 → θJA = 180 °C/W**. Con OTP a 120 °C y 40 °C de ambiente,
cada PAM8302A sólo puede disipar **0,44 W** — que es exactamente lo que disipa dando
sus 2,5 W nominales. **Un buck más grande no habilita la potencia plena**; eso exigiría
migrar a `PAM8302AAYCR` (U-DFN3030, θJA 47,9) con pad térmico y vías bajo los 32.
Decisión grande: hay que tomarla explícitamente, no dejarla implícita en el ítem del buck.

### 0.4. No hay fusible ni protección de polaridad inversa — y el jack barrel lo empeora
En toda la placa no hay un solo MOSFET ni fusible. El SMBJ24A es unidireccional: ante
polaridad invertida conduce en directa y queda como un corto. **El ítem 6b (jack barrel)
es la principal fuente de errores de polaridad que existe**; ponerlo sin protección es
la combinación que quema placas de laboratorio. La serigrafía no es protección.

⚠️ **Topología**: J11 y J12 están en el mismo net `+24V` — la placa es un **nodo de paso**
del daisy-chain. Por eso el fusible y el P-FET **no van en serie con la entrada** (cortarían
la cadena entera), sino en una **derivación** que alimenta el buck local.

---

### 0.5. 🔴 Las filas están al DOBLE de separación que las columnas — el eje vertical no enfoca

**Verificado a oído el 2026-08-19**: en el modo 15 se percibe el movimiento del foco
en horizontal, algo en profundidad, y **nada en vertical**. No es percepción: es la
geometría del array.

| a 2 kHz (λ = 172 mm) | pitch | |
|---|---|---|
| horizontal — 8 columnas, **80 mm** | 0,47 λ | ✅ bajo el límite λ/2 |
| **vertical — 4 filas, 160 mm** | **0,93 λ** | ❌ **lóbulos de rejilla** |

El criterio de muestreo espacial es **d ≤ λ/2**. Con 160 mm entre filas el techo
vertical es **1072 Hz**, la mitad del horizontal (2144 Hz). A 2 kHz el enfoque
vertical está *aliaseado*: en vez de un foco produce varios lóbulos repartidos, y
por eso no se percibe ningún movimiento al cambiar la altura del foco.

Además hay **la mitad de elementos** en vertical (4 filas contra 8 columnas), así
que aunque se respetara λ/2 la resolución vertical sería peor.

**Decisión para DS12 — es una decisión de producto, no un fix:**

- **Si el eje vertical importa** (colocar fuentes a distintas alturas, que es
  bastante del atractivo de la intervención): las filas tienen que ir a **≤ 86 mm**
  de pitch para 2 kHz. Eso implica repensar el layout mecánico — hoy 160 mm viene
  del tamaño del parlante y su montaje.
- **Si no importa**: dejarlo documentado y **no prometer enfoque vertical**. El
  array es esencialmente **lineal horizontal con algo de apertura vertical**, y
  conviene decirlo así en la comunicación del proyecto.
- **Camino intermedio**: aceptar que el enfoque vertical sólo existe **por debajo
  de ~1 kHz**, y usar la altura sólo con material grave.

> Esto no se puede arreglar por firmware. Los delays ya se calculan bien en 3D
> (`Beam_InitTxGeometry` + `Beam_SetTxFocus`); lo que falla es que el array no
> tiene resolución vertical a esa frecuencia.

#### El experimento que decide entre las tres opciones: **modo 17** (2026-08-20)

Antes de rediseñar el layout mecánico conviene saber **cuál de los dos límites
manda**, porque piden cosas distintas y sólo uno se arregla bajando el pitch:

- ¿el **aliasing** (pitch de 160 mm)? → bajar el pitch alcanza.
- ¿la **apertura** (sólo 4 filas, 480 mm)? → hay que **agregar filas**, y bajar
  el pitch sin agregarlas no cambiaría nada.

El modo 17 los separa: barre el foco en vertical con un tono de **800 Hz**, donde
el pitch de 160 mm son 0,37 λ — **sin lóbulos de rejilla en todo el rango de
steering**. Si a 800 Hz se percibe movimiento vertical y a 2 kHz no, el limitante
es el aliasing y el rediseño se justifica. Si tampoco se percibe, el limitante es
la apertura: a 800 Hz son 1,12 λ → haz de ~51°, spot de ±45 cm a 1 m.

**Es una medición de una tarde que decide gasto de hardware.** Pendiente de
correr. Procedimiento en `docs/manual_modos.md`, sección "Modo 17".

### 0.6. 🔴 Seis de los siete puntos de prueba de la DS11 no existen eléctricamente

> **Corregido el 2026-08-21**: la versión anterior decía "los siete". **J13 sí
> tiene cobre**: 16 segmentos en F.Cu que llegan a R82 y a los pines VOUT1± de
> U4 (PCM1690, DAC3) — o sea la salida del canal 24 antes del amplificador. Lo
> que no tiene es **red asignada** (`net -1`), y por eso no aparece en el
> netlist. El pad 2 sí está suelto. Detalle en `docs/loopback_debug.md` §1.

**Verificado sobre `DS.kicad_pcb` el 2026-08-20.** La placa tiene siete huellas
pensadas para debug —**J13** (header 1x02) y **TP1 a TP6** (pads de 1,5 mm)—.
**TP1 a TP6** están todos en el mismo estado:

- marcadas `dnp` + `exclude_from_bom` + `exclude_from_pos_files`
- **pads sin red asignada**
- **ninguna pista de cobre termina a menos de 0,6 mm de ellos**

O sea: se dibujaron, nunca se conectaron, y la fábrica ni se enteró de que
existían. **La DS11 no tiene un solo punto donde pinchar una señal.**

Duele especialmente en J13, que en el esquemático sí está cableado a
`VD.P1/VD.N1` —la salida diferencial del DAC antes del amplificador—, que es
exactamente la señal que uno querría medir.

**Para la DS12:**

1. **Verificar que cada TP tenga red y cobre antes de plotear.** Es el mismo
   tipo de falla que los cortos de fábrica de la DS11: el diseño decía una cosa
   y el archivo que fue a producción decía otra. Un DRC no lo marca porque un
   footprint sin red no viola ninguna regla.
2. **Un header de lazo cerrado de fábrica.** Un canal de cada uno de los 4
   bancos, con divisor y pasabajos **en la placa**, llevado a un header junto a
   las entradas. Convierte medir el SYNC entre bancos, los retardos
   fraccionales y el mapeo de canales en "enchufar un cable plano".
   Fundamento y valores en `docs/loopback_debug.md`.

### 0.7. 🔴 La ganancia del PAM8302A obliga a tirar 42 dB de rango dinámico

`R_I = 1 kΩ` da `A_V = 2·(142k/1k) = 284` = **+49 dB**. Para que el amplificador
no recorte, la entrada tiene que quedar en ~11 mV<sub>rms</sub>, y como el
PCM1690 entrega ~1,4 V<sub>rms</sub> a fondo de escala, hay que atenuar
**≈ −42 dB en digital**. Son 42 dB de rango dinámico del DAC que se tiran.

Corroboración independiente: Fernando venía operando a **−46 dB por oído**.

Subir `R_I` a 20 kΩ (ganancia +23 dB) subiría el punto de operación digital
~26 dB, con la mejora de señal-ruido correspondiente. **Y de paso corre el
pasaalto de entrada de 723 Hz a 36 Hz**: hoy el modo 17 (barrido vertical,
800 Hz) trabaja a media octava del borde de la banda, con −2,6 dB y 42° de fase.

> Mientras tanto, en la DS11: el default de fábrica de −24 dB deja el
> amplificador **12 dB dentro del recorte** con los modos de tono. Debería ser
> −40 dB.

## 🔴 Prioridad alta

### 1. Margen del buck a 24V — el TVS NO protege al MP2315S
Verificado con el datasheet del D8 (SMBJ24A, C908801) el 2026-07-08:
- SMBJ24A: standoff **24V**, VBR(min) **26.7V**, **VC (clamping) 38.9V** @ 15.4A, 600W, unidireccional. Parte correcta.
- Pero **MP2315S abs max = 26V** → está **por debajo incluso del VBR (26.7V)** del TVS. En un transitorio, el buck muere antes de que el TVS empiece a conducir, y el clamp real (38.9V) está muy por encima. **El TVS no da protección útil al buck en operación a 24V.**
- **Mitigación ya decidida para DS11** (§8.3): alimentar el sistema a **19-20V**. Sigue vigente.
- **Para DS12, elegir una de**:
  - Buck de mayor tensión de entrada (abs max ≥36V; p.ej. familia 36V/60V) → recupera margen real a 24V y permite el daisy-chain a 24V sin caminar sobre vidrio.
  - Si se fija 19-20V como tensión de sistema definitiva: TVS de standoff más bajo (SMBJ18A/20A) que clampee por debajo de los 26V del buck → ahí sí protege.
  - Bulk/snubber de entrada dimensionado para el transitorio del hot-plug del daisy-chain.

### 2. Tap de línea VOUT + R 1kΩ — NO entró en la DS11
Estaba planificado (DS11_PLANNING §8.2) pero **no aparece en el BOM final** de la DS11
(ni la R de 1k ni el header). Se perdió en el cierre. Reincorporar en DS12:
- Header 2 pines **DNP** desde VOUT ch0 del PCM1690 (antes del PAM) vía **R serie 1 kΩ** + GND.
- Habilita: self-test de toda la cadena DAC→ADC (chirp + correlación del AUTOFOCUS),
  verificación empírica de la latencia del nodo, y el **"oscilador en la cabecera"**
  (un nodo genera contenido y lo inyecta al daisy-chain analógico → los demás lo
  beamformean coherente). Ver [../../docs/multinode.md](../../docs/multinode.md) §2.
- Costo: 2 pads + 1 R.

### 2b. J7 pin 1 (3V3 del ST-Link) sin protección — poner Schottky
Detectado en el bring-up de la DS11 (2026-08). El **pin 1 de J7 va directo al riel
`+3V3`**, mientras que el pin 10 (5V) sí pasa por el Schottky **D11**. Sin esa
protección el ST-Link intenta alimentar toda la placa (STM32H743 + 4 PCM1690 + AK5538
+ ESP32 ≈ 700 mA), no puede, y se apaga su LED — que es exactamente el síntoma que
hizo perder horas creyendo que había un corto. Mitigación en DS11: **cortar el pin 1
del cable**. Para DS12: **Schottky en serie en el pin 1**, igual que el D11 del pin 10.
Detalle en [../../docs/ds11_errata.md](../../docs/ds11_errata.md).

### 2d. JSPK09 y JSPK10 están cruzados en el esquemático — corregir
Confirmado en el netlist (`DS.txt`) y **verificado en la placa física el 2026-08-14**:
Fernando tuvo que intercambiar los dos cables a mano para que el channel walker
recorriera la matriz en orden.

Todos los conectores toman grupos consecutivos de 4 ferrites de salida, **salvo
estos dos**:

| conector | ferrites | amplificadores |
|---|---|---|
| JSPK07 | FB24-27 | |
| JSPK08 | FB28-31 | |
| **JSPK09** | **FB36-39** | **U28, U29** ← |
| **JSPK10** | **FB32-35** | **U26, U27** ← |
| JSPK11 | FB40-43 | |
| JSPK12 | FB44-47 | |

En términos de canal: VC (U1/DAC2, SAI2_A ch16-23) manda ch1-2→JSPK10 y
ch3-4→JSPK09.

**Ojo con el historial**: esto se detectó en el análisis de netlist del
2026-07-02, **antes de fabricar**, y se cerró en
[DS11_PLANNING.md](DS11_PLANNING.md) §143 como *"RESUELTO POR DISEÑO"* porque se
decidió absorberlo en el orden del arnés. La decisión era legítima, pero la
convención del arnés no quedó escrita en ningún lugar que se cruzara al momento
de armar — así que en la práctica costó una sesión de debug igual.

**Para DS12: corregirlo en el esquemático.** Es gratis, y elimina una trampa que
se paga cada vez que alguien arma un arnés nuevo. Si por alguna razón se decide
mantenerlo, tiene que estar **serigrafiado en la placa**, no sólo en un doc.

### 2c. Regenerar gerbers CON refill de zonas — regla de proceso
La tirada entera de DS11 salió con **dos cortocircuitos de cobre** porque los gerbers se
plotearon con los rellenos de zona desactualizados. No es un cambio de PCB: es una regla
de proceso obligatoria antes de mandar a fabricar (refill + doble DRC comparado). El
procedimiento exacto está en [../../docs/ds11_errata.md](../../docs/ds11_errata.md).

---

## 🟡 Prioridad media

### 3. Línea de timing GPIO4→PC13 (ESP32→STM32)
Hedge opcional analizado el 2026-07-07 (verificado en netlist). No entró en DS11.
Para DS12, si se quiere un disparo de timing sin jitter entre procesadores:
GPIO4 (U5 pin 18) → PC13 (U7 pad 7) con **R serie ~220 Ω** cerca del ESP32; PC13 como
entrada/EXTI con pull interno (activa-baja). **No rutear la inversa** — el UART ya es
interrupt-driven en ambos sentidos. Ver [../../docs/esp32_roles.md](../../docs/esp32_roles.md).

### 4. Ferrites de salida de los PAM (64 posiciones) — solo si se quiere full power
Hoy: un solo bead genérico (C21286, "FerriteBead" 0805) para las 67 posiciones,
incluidas las 64 de salida de parlante (2 por parlante) + FB2/FB3 de potencia del +5VA.
Para el uso real (muy por debajo del full output) **está bien** — la corriente es chica,
no satura, filtra bien. **Solo si DS12 va a correr a plena potencia**: especificar para
las 64 de salida un bead de clase-D con **corriente nominal ≥1.5-2 A y DCR ≤0.1 Ω**
(el genérico de señal ~0.3 Ω/200-500 mA saturaría y perdería filtrado a ~1 A de pico).
Dejar el chico solo donde la corriente es de mA (FB1/VREF+).

### 5. C45/C46 en 0201 → pasar a 0402
Son los caps de carga del cristal de 32.768 kHz (8.2 pF, valor correcto). Es el **único
0201 de toda la placa** (el resto es 0402+). Migrar a 0402 mismo valor: más fácil de
montar, retrabajable a mano, no fuerza proceso. Footprint heredado.

### 6b. Barrel jack DC de entrada, **DNP** — para banco y experimentación
Pedido de Fernando (2026-08-13). Hoy la entrada de alimentación son **borneras Phoenix**,
que son lo correcto para instalación fija pero incómodas en la mesa: cada vez que se
quiere probar con una fuente comercial (notebook-style, bench supply con plug) hay que
pelar cable y atornillar. Agregar en paralelo un **jack barrel DNP** (sin poblar de
fábrica) que se puede soldar sólo en las placas de laboratorio.

- **Footprint**: DC-005 / barrel 5.5 × 2.1 mm, through-hole (el 2.1 mm es el estándar de
  las fuentes tipo notebook y de los bench supplies con plug).
- **En paralelo con la Phoenix de entrada**, mismo net, antes del TVS y del buck — así
  toda la protección existente (D8, fusible si lo hay) cubre las dos entradas por igual.
- **Tensión**: la placa corre a **19-20 V** (ver ítem 1), que es justo el rango de los
  cargadores de notebook — encaja natural. Marcar la serigrafía con **"19-20V"** y la
  polaridad (centro positivo) para no quemar el buck con una fuente de 24 V.
- ⚠️ **No poblar los dos a la vez** con fuentes distintas conectadas: serigrafiar
  "USE ONE" o similar. Dos fuentes en paralelo por el mismo net es un problema.
- Costo: un footprint THT y un pad extra. DNP = cero costo en la tirada de producción.

### 6c. Sensor de temperatura ambiente → corregir la velocidad del sonido
Pedido de Fernando (2026-08-14). Hoy el firmware usa
`SPEED_OF_SOUND_MM_S = 343000.0f` hardcodeado, que corresponde a **~20 °C**.

La física: `c ≈ 331,3 + 0,606·T` m/s → **+0,18% por °C**.

**Cuánto importa realmente** (vale la pena tenerlo antes de gastar un footprint):
lo que gobierna el enfoque no es el delay absoluto sino el **diferencial** entre
elementos. Para un foco a 1 m, el elemento de esquina (300, 240 mm) está a
1071 mm contra 1000 mm del centro → **71 mm ≈ 208 µs** de diferencial máximo.

| ΔT respecto de 20 °C | error en el diferencial | fase a 2 kHz | fase a 8 kHz |
|---|---|---|---|
| 10 °C | 3,7 µs | 2,7° | 10,7° |
| 30 °C (exterior) | 11 µs | 8° | 32° |

O sea: **para uso en interior el efecto es de segundo orden** (a 2 kHz, 2,7° de
error no mueve el foco de forma audible). Empieza a importar en instalaciones a
la intemperie con swing grande de temperatura, y sobre todo en la parte alta del
espectro.

**Recomendación**: el 80% del beneficio no necesita hardware nuevo — alcanza con
**exponer la velocidad del sonido como parámetro de runtime** por HTTP y cargarla
a mano según la temperatura del lugar. El sensor sólo compra la automatización.

⚠️ **No se puede usar el sensor interno del STM32 como proxy**: mide temperatura
de juntura y corre 15-20 °C por encima del ambiente (en el soak marcaba 39-44 °C
con la sala a ~22 °C).

Si igual se pone sensor, conviene uno I2C de bajo costo lejos de los rieles de
potencia y del disipador del buck. Y sirve además para vigilancia térmica de una
instalación desatendida, que es un argumento independiente y quizás más fuerte
que el acústico.

### 6. RN5/RN6 y arrays en general → considerar discretas
Ya hubo que reemplazar RN1-4 (62Ω sin stock) por 16 discretas de 75Ω. RN5/RN6 (C1980,
4.7k array) siguen siendo arrays. Para DS12, evaluar discretas de entrada: menos riesgo
de rotación en JLCPCB y de quiebres de stock. (Trade-off: área.)

### 6d. Pinout de J9: 2 de cada 4 canales NO caen sobre pares trenzados
Detectado el 2026-08-16 trazando el netlist para escribir la guía de operación.

La placa empareja los pines del RJ45 de forma **consecutiva**:

| pines J9 | entrada | cap de acople | AK5538 |
|---|---|---|---|
| 1(+), 2(−) | 1 | C309/C308 | AIN1P/N |
| 3(+), 4(−) | 2 | C307/C306 | AIN2P/N |
| 5(+), 6(−) | 3 | C305/C304 | AIN3P/N |
| 7(+), 8(−) | 4 | C303/C302 | AIN4P/N |

(y las entradas 5-8 igual, en los jacks 3/4 — pines 25-32 / 37-44.)

Pero **un cable de red estándar (T568A/B) trenza los pares (1,2), (3,6), (4,5),
(7,8)**. O sea:

- entrada 1 = pines 1,2 → ✅ es un par trenzado real
- entrada 2 = pines 3,4 → ❌ pin 3 es del par verde, pin 4 del azul
- entrada 3 = pines 5,6 → ❌ pin 5 es del azul, pin 6 del verde
- entrada 4 = pines 7,8 → ✅ es un par trenzado real

**Consecuencia**: en las entradas 2 y 3 la señal diferencial viaja por dos pares
distintos → se pierde el rechazo de modo común y sube la diafonía. Con tiradas
cortas no se nota; con cables largos sí, y justo esas entradas son las que van a
llevar señal de programa en la intervención de 8 canales.

**Para DS12**: remapear a **(1,2) / (3,6) / (4,5) / (7,8)** para que las cuatro
entradas caigan sobre pares trenzados reales. Es sólo ruteo, cero costo de BOM.

**Mientras tanto (DS11)**: usar las **entradas 1 y 4** de cada jack para las
tiradas más largas o las señales más delicadas. Anotado en
[../../docs/guia_operacion.md](../../docs/guia_operacion.md) §5.

### 7. Serial number: la etiqueta salió con "JLCJLCJLCJLC" literal
En la tirada de DS11 se puso el placeholder de JLCPCB pero **no lo reemplazaron por
un número de serie** — quedó el texto literal impreso. Puede ser un error de
configuración del pedido (el servicio de serialización suele ser un add-on que hay
que tildar explícitamente, aparte de poner el marcador en la serigrafía).

**Para DS12**: al armar el pedido, verificar en el preview de JLCPCB que el campo
del marcador aparezca resuelto y no como literal. Si el servicio de serialización
no está disponible o encarece, la alternativa es **dejar un recuadro serigrafiado
vacío** y numerar a mano con marcador indeleble o etiqueta — que para tiradas de 5
a 10 placas es más barato y no depende del proveedor.

---

## 🟢 Verificaciones al hacer bring-up de la DS11 (no son cambios de PCB)

- **AP2125 (U10, C110463)**: confirmar en el bring-up que el rail 3.3VA mide **3,3V**
  (el comment del BOM decía solo "AP2125"; verificar que la parte enviada es la variante
  -3.3 y no otra tensión fija). Para DS12: fijar el C-number exacto del AP2125-3.3.
- **10µF cerámicos (C1691)**: si son de 10V, el derating por DC bias a 5,15V deja
  ~5-6µF efectivos. Aceptable para decoupling; solo tenerlo presente si algún rail lo
  necesita nominal.
- **Sincronía BOM ↔ doc de planning**: el D8 salió con C908801 (correcto) pero el doc
  tenía C726830. Mantener los C-number del BOM igual que el planning para no repetir el
  mecanismo por el que entró el SM6T15A en la tanda 1.

---

## ✅ Confirmado OK en la DS11 (no tocar)
- Sustituciones de BOM validadas: 100pF→C327198 (YAGEO NP0 ±1%), 4.7nF→C2961271
  (**C0G confirmado por Fernando**), RN1-4→16×75Ω discretas (R133-148, R131=33Ω intacto
  en MCLK), electrolíticos en rieles ≤5V salvo C56 (50V en +24V), divisor buck 40.2k/7.5k.
- D8 = SMBJ24A unidireccional legítimo (datasheet verificado).
- Rotation corrections aplicadas: JST_SH_BM10B=180, AP2125/SOT-23-5, PRTR/SOT-143=90,
  220µF/CP_Elec_6.3x7.7=180.
