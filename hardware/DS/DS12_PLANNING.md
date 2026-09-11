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

#### ✅ 2026-09-02 — la solución: dos bucks desde la entrada, y el 3V3 deja de ser un LDO

> 📄 **La especificación completa, lista para dibujar en KiCad, está en
> [DS12_ALIMENTACION.md](DS12_ALIMENTACION.md)**: esquemático nodo por nodo, BOM, tabla de
> modos de operación, análisis de fallas, reglas de layout y procedimiento de verificación.
> Lo de abajo es el resumen.

Análisis completo del conexionado de alimentación hecho sobre `DS.txt` (netlist de
producción) + datasheets. Lo que sigue reemplaza al "buck propio para el riel digital"
genérico de arriba por una topología concreta con part numbers.

##### El árbol de alimentación real de la DS11 (verificado en el netlist)

```
J11 ─┬─ +24V ─┬─ D8 (SMBJ24A, TVS)            ← J11 y J12 son EL MISMO net: nodo de paso
J12 ─┘        ├─ C56 22µF/50V + C58 100nF        del daisy-chain. Sin fusible, sin P-FET.
              ├─ R33 470k → EN (sin UVLO programado)
              └─ IC2 MP2315S ─ SW ─ L1 4.7µH ─┬─ +5VA  5,09 V
                                              │   (FB: 0,8V × (1+40,2k/7,5k) = 5,09 V;
                                              │    R31 20k es serie a FB, C62 33pF feedfwd)
                                              │
    +5VA ─┬─ 32× PAM8302A (U12-U43)  +  bulk 5×220µF (C71, C290-293)
          ├─ FB3 ─ +5VA_DAC ─ 4× PCM1690 VCC1/VCC2
          └─ FB2 ─ D9 (SS34) ─┬─ +5V ─┬─ U11 ─ +3V3   ← toda la carga digital
                              │       ├─ U10 ─ +3,3VA
                              │       └─ J5.10 / J6.10 (expansión, al exterior)
                              │
      USB-C J3 VBUS ─ D2 ─────┤   ← los tres diodos hacen OR sobre +5V
      J7.10 (ST-Link 5V) ─ D11┘

      J7.1 (ST-Link 3V3) ─────────── +3V3   ⚠️ SIN NADA EN SERIE
```

##### Los seis hallazgos, en orden de gravedad

**1. El margen de dropout no es de 250-350 mV: es de cero.** La `presupuesto_potencia.md`
§3bis B lo calculó con el **AP1117**, pero el part number que está en el BOM es otro:

| ref | símbolo dice | BOM / LCSC dice | dato que importa |
|---|---|---|---|
| U11 | AP1117-33 | **AMS1117-3.3** (C6186) | dropout **1,1 V típ / 1,3 V máx** |
| U10 | AP2125 | **AP2125K-3.3TRG1** (C110463) | 300 mA, VIN máx 6 V, dropout **65 mV**, PSRR 65 dB |

Con ~0,6 A de riel digital: `+5V = 5,09 − 0,35 (D9) − 0,13 (FB2, 220 mΩ) = 4,61 V`.
El AMS1117 necesita `3,3 + 1,1 = 4,4 V` típico y **`4,6 V` en el peor caso**. O sea que
la placa nominal está **a 10 mV del borde**, y cualquier hundimiento de `+5VA` la cruza.
Eso ya no es "poco margen", es una cadena que no cierra por diseño. (El AP2125 con sus
65 mV de dropout nunca perdió regulación — por eso el ADC nunca fue el síntoma.)

Y de paso queda cerrada la verificación pendiente del bring-up: **U10 es un LDO fijo de
3,3 V**, así que el riel `+3,3VA` tiene que medir 3,3 V; si mide otra cosa, hay un problema.

**2. Cuatro fuentes en paralelo sobre el riel digital, tres protegidas y una no.**
`+5V` recibe de `+5VA` (D9), del USB-C (D2) y del ST-Link (D11) — los tres Schottky
bien orientados, ninguna de las tres puede backfeed al VBUS ajeno. Pero **J7 pin 1 va
directo a `+3V3`**, sin diodo ni resistencia: es un cable duro entre el riel de la placa
y el pin de 3,3 V de la sonda. Ver el ítem 2b, que hay que reformular (abajo).

**3. Hot-plug.** 🔧 *(corregido 2026-09-11: **C56 NO es cerámico, es un electrolítico de
aluminio** de 22 µF/50 V — `C72505`, footprint `CP_Elec_6.3x5.4`, 43 mA de rizado @120 Hz.
La derating por DC bias que decía este párrafo **no aplica**: 22 µF son 22 µF, y la DS11 ya
tiene amortiguación en la entrada. Ver [DS12_AUDITORIA.md](DS12_AUDITORIA.md) §4.)*
El bulk de entrada es C56 + C58. Enchufar en
caliente un ladrillo de 19-20 V por un cable largo es un LC serie sin amortiguar:
**el pico llega a ~2× la tensión de entrada**, o sea 40 V. Eso está por encima del máximo
absoluto del MP2315S (26 V) **y** de cualquier buck que pongamos. El TVS clampea a 38,9 V,
que tampoco alcanza. Falta un electrolítico/polímero con algo de ESR en la entrada
(100 µF/35 V) — es amortiguación, no filtrado, y hoy no está.

**4. El MAX809T está mal elegido para un riel de 3,3 V.** Umbral 3,08 V típico con rango
**2,92-3,23 V**. Un buck de 3,3 V ±2 % puede quedar en 3,234 V: por encima del típico,
pero **por debajo del peor caso del supervisor**. Hoy zafa porque el LDO tira un poco
alto; con el riel bien regulado el margen sigue siendo malo. → **MAX809S (2,93 V)**.
El STM32H743 funciona hasta 1,71 V, así que 2,93 V sobra y deja 300 mV de margen real.

**5. J5/J6 sacan `+3V3` y `+5V` al exterior sin ninguna protección** (pines 9 y 10).
No hay fusible, ni PTC, ni TVS. Un cortocircuito en un cable de expansión arrastra el
riel del MCU. Mínimo: PTC de 500 mA en cada uno, o al menos serigrafía y una nota.

**6. La UVLO del buck no está programada.** R33 (470 k) sólo tira EN a VIN. Con la
cadena cayendo, el MP2315S sigue intentando arrancar y entra en hiccup en vez de
apagarse limpio. El EN de los AP632xx tiene umbral de precisión (1,18 V) justo para eso.

##### La topología propuesta para DS12

```
J11/J12 (19-20 V, nodo de paso) ──────────────────────► siguiente placa (sin fusible en serie)
   └─ derivación local: F1 3A ─ P-FET pol. inversa ─ D8' SMBJ20A ─ C 100µF/35V elec + 22µF cer
        ├── BUCK A   AP63356DV-7   5,12 V / 3,5 A ──► +5VA ─┬─ 32× PAM8302A
        │            (UVLO a ~15 V: los amps caen primero)  └─ FB3 ─ +5VA_DAC
        │
        └── BUCK B   AP63201WU-7   3,30 V / 2 A ────► +3V3  (STM32, ESP32, 4× PCM1690 VDD,
                     ▲                                       AK5538 TVDD, EEPROM, pull-ups)
                     │
              D20 ───┘  Schottky desde +20V
              D21 ───┘  Schottky desde +5V_AUX      ◄── el OR va en la ENTRADA del buck

   +5VA ─ FB2 ─ D9 ─┬─ +5V_AUX ─┬─ U10 AP2125 ─ +3,3VA        ← ahora sólo ~60 mA:
   USB-C VBUS ─ D2 ─┤            ├─ J5.10 / J6.10                la caída de FB2+D9
   J7.10 ─ D11 ─────┘            └─ D21 ─ VIN de BUCK B          pasa de 0,48 V a 0,02 V

   J7.1 ─ 100 Ω ─ +3V3        ← VTref: sensa, no alimenta (ver 2b reformulado)
```

**La idea que hace que todo esto sea barato: el OR de fuentes se muda de la SALIDA del
regulador a su ENTRADA.** El AP63201 arranca desde 3,8 V, así que el mismo buck sirve
con 20 V (modo normal) y con los 4,6 V que llegan del USB (modo flasheo sin fuente).
Consecuencias:

- **U11 (AMS1117) desaparece del BOM.** Con él se van 1,2 W de calor dentro de la caja
  y el único componente de la cadena que no cerraba por dropout.
- **La protección del puerto USB no se toca**: siguen siendo D2 y D11, en el mismo lugar,
  haciendo lo mismo. Lo único que cambia es que ahora alimentan un nodo auxiliar de 5 V
  en vez del riel principal.
- **D20 es obligatorio**, no cosmético: sin él los 5 V del USB subirían al net `+24V` y
  de ahí a **todas las placas de la cadena** y a la entrada del buck de los amps.
- **D21 protege al AP2125**, cuyo máximo absoluto de entrada son 6 V: sin él los 20 V
  del VIN del buck B volverían a `+5V_AUX`.
- En modo USB el buck rinde ~90 % en vez del 66 % del LDO: **0,38 A desde el VBUS en vez
  de 0,6 A**. Aun así, para bring-up sin fuente conviene un cargador, no un puerto de
  laptop limitado a 500 mA.
- La UVLO programada va en el **buck A** (amps), no en el B. Así, con la cadena cayendo,
  **los amplificadores se apagan primero y el MCU sigue vivo para reportarlo** por red.

##### El integrado: AP63201WU-7 (LCSC C2071044)

Diodes Inc., misma familia que el AP2125 que ya está en la placa. TSOT-26, ~US$ 0,25.

| parámetro | valor | por qué importa acá |
|---|---|---|
| VIN recomendado | 3,8 – 32 V | cubre 20 V normal **y** 4,6 V del USB con el mismo chip |
| VIN máximo absoluto | **35 V DC / 40 V por 400 ms** | contra los 26 V del MP2315S: ver la coordinación con el TVS abajo |
| Corriente | 2 A continuos | el riel digital estimado es 0,55-0,9 A → 2× de margen |
| fSW | **500 kHz** | con 20 V→3,3 V el duty es 16,5 % → tON = 330 ns contra 80 ns de mínimo (**4×**) |
| Modulación | **PWM siempre, sin PFM** | *esta es la razón principal de elegir el '201* |
| Ref / salida | 0,8 V ±1 %, ajustable | divisor 75 k / 24 k → **3,300 V** exactos |
| EN | umbral de precisión 1,18 V | UVLO programable por divisor (Eq.1/Eq.2 del datasheet) |
| Protecciones | límite de pico ciclo a ciclo, hiccup, OVP, shutdown térmico | |
| θJA | 89 °C/W (TSOT26) | a 0,6 A: Pd ≈ 0,33 W → +29 °C. Con 40 °C de ambiente, TJ ≈ 69 °C ✅ |

**Por qué el '201 y no el '203 (que es 3,3 V fijo y parece más simple):** el AP63203 es
1,1 MHz **y tiene PFM**. Las dos cosas juegan en contra:

1. A 1,1 MHz con 20 V de entrada el tON baja a 150 ns, y a 24 V a 125 ns, contra 80 ns de
   mínimo. Al borde del salteo de pulsos — que es rizado de baja frecuencia, justo lo que
   no queremos.
2. PFM = burst a carga liviana. **Es exactamente el mismo mecanismo del AAM del MP2315S**
   que documentamos en `presupuesto_potencia.md` §3bis C y que se arregla removiendo R36.
   No tiene sentido arreglarlo en un buck y reintroducirlo en el otro.

El precio de elegir el '201 es un divisor de dos resistencias. Barato.

**BOM alrededor del buck B:**

| | valor | nota |
|---|---|---|
| L | 10 µH, Isat ≥ 2,5 A, DCR < 50 mΩ | ΔIL = 0,6 A a 20 V. Sunlord MWSA0703S o similar (ya usamos MWSA0603S en L1) |
| CIN | 2× 10 µF / 50 V X7R 1210 + 100 nF | derating por DC bias: a 20 V un X7R 1210 pierde ~40 % |
| COUT | 2× 22 µF / 10 V X7R 0805 + 100 nF | |
| CBST | 100 nF / 16 V | de BST a SW |
| R1 / R2 | 75 kΩ / 24 kΩ 1 % | `VOUT = 0,8 × (1 + 75/24) = 3,300 V` |
| CFF | 22 pF sobre R1 | transitorio (el datasheet admite 10-220 pF) |
| D20, D21 | SS34 o similar | OR de entrada; D20 conduce ~130 mA, caída despreciable |

##### Y de paso, el buck de los amplificadores (ítem 1 / 0.2): AP63356DV-7 (LCSC C2158014)

No es lo que preguntaste, pero sale gratis del mismo análisis y resuelve tres ítems abiertos:

- **3,5 A** contra 3 A del MP2315S.
- **θJA = 25 °C/W** (V-DFN3020 con pad térmico) contra **100 °C/W** del MP2315S. El límite
  de 1,25 W de disipación continua de `presupuesto_potencia.md` §1 —el que hacía elegir
  entre 12 V y 19 V— deja de ser el binding constraint.
- **PWM siempre, sin PFM.** El ítem del AAM/R36 desaparece estructuralmente, no por
  remover un componente.
- Tiene **Power Good**, que el STM32 puede leer por GPIO: discriminador de brown-out del
  riel de amps por hardware, sin inferirlo del `reset` del `/status`.
- Mismo fabricante, misma familia de ecuaciones y mismo estilo de layout que el buck B.
  Divisor para 5,12 V: R1 = 162 k, R2 = 30 k. L = 6,8-10 µH.

##### Esto reabre §0.2 (el TVS): con un buck de 35 V la coordinación cierra

La tabla de §0.2 se calculó contra el **máximo absoluto de 26 V del MP2315S** y por eso
daba imposible. Con la familia AP632xx (35 V DC / 40 V por 400 ms) el cuadro cambia:

| TVS | VRWM | VC máx | ¿bajo 35 V DC? | ¿bajo 40 V / 400 ms? |
|---|---|---|---|---|
| SMBJ20A | 20 V | 32,4 V | **sí** ✅ | sí |
| SMBJ22A | 22 V | 35,5 V | al borde | sí |
| SMBJ24A (actual) | 24 V | 38,9 V | no | sí, con 1,1 V de margen |

**Recomendación: fijar el sistema en 19 V (ladrillo de notebook típico: 19,0-19,5 V) y
poner SMBJ20A.** Queda 2,6 V por debajo del máximo absoluto **continuo**, no del de
transitorio — que es el único caso en que un TVS realmente protege. Si se quiere admitir
hasta 21 V de entrada, SMBJ22A y se acepta que la coordinación es sólo con el rating de
400 ms. **La conclusión de §0.2 —"hace falta un buck de ≥42 V"— era correcta *dado el
SMBJ24A*; bajando el TVS a SMBJ20A un buck de 35 V alcanza y sobra.**

Sigue haciendo falta la impedancia serie entre conector y TVS (el fusible del §0.4).

##### Presupuesto del riel de 3,3 V (para dimensionar el buck B)

| consumidor | típico | pico |
|---|---|---|
| STM32H743 @480 MHz VOS0 | ~300 mA | ~350 mA |
| ESP32-C3 con WiFi | ~100 mA | ~350 mA (TX) |
| 4× PCM1690 VDD (digital) | ~100 mA | ~120 mA |
| AK5538 TVDD + LDO interno | ~15 mA | |
| EEPROM, pull-ups, LEDs, e-paper | ~30 mA | |
| **total** | **≈ 550 mA** | **≈ 900 mA** |

2 A es el tamaño correcto: 2× sobre el pico y sin pasarse de temperatura en TSOT-26.
Nota: liberar esa carga de `+5VA` le devuelve **~0,6 A** al buck de los amplificadores
(hoy el LDO consume 3 W del riel de 5 V para entregar 2 W).

##### Lo que hay que verificar en banco antes de congelar el diseño

- [ ] Medir el `+5V` real de una DS11 con los amps encendidos y a volumen. Si baja de
      4,6 V, el hallazgo 1 queda cerrado con número.
- [ ] Medir `+3,3VA` = 3,3 V (cierra la verificación pendiente sobre U10).
- [ ] Confirmar el consumo real del riel de 3,3 V con un shunt, contra los 550 mA de la tabla.
- [ ] Verificar el pinout del AP2114H-3.3 (SOT-223) contra el AMS1117 **si** se decide
      dejar el LDO en vez de sacarlo — no dar por hecho que es drop-in.

### 0.2. El TVS: con 19-20 V y un buck de 26 V no existe solución

> ⚠️ **2026-09-02: este ítem quedó reabierto por §0.1.** La imposibilidad de abajo se
> deriva del máximo absoluto de **26 V del MP2315S**. Con la familia AP632xx (35 V DC /
> 40 V por 400 ms) el **SMBJ20A** cierra la coordinación. Ver la tabla al final de §0.1.

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

### 0.7. ⚠️ RETIRADO — se apoyaba en números que ya estaban corregidos

> **Estado 2026-09-02: este ítem NO se hace como estaba escrito.** Fernando
> preguntó si los 723 Hz eran ciertos y si no lo habíamos descartado ya. Lo
> habíamos descartado: `loopback_debug.md` **§19.3**, medido. Este ítem se
> escribió desde §2.1, que §19.3 corrige, y la corrección nunca llegó hasta acá.

**Lo que decía (desde §2.1, derivado del esquemático):** `R_I = 1 kΩ` da
`A_V = 2·(142k/1k)` = **+49 dB**, obliga a atenuar −42 dB en digital, y deja el
pasaalto de entrada en **723 Hz**. Propuesta: subir R_I a 20 kΩ.

**Lo que dice la medición (§19.3, respuesta del amplificador solo contra J13):**

| Hz | 20 | 30 | 45 | **63** | 90 | 125 | 250 | 500 | **723** | **800** |
|---|---|---|---|---|---|---|---|---|---|---|
| dB | −10,8 | −7,9 | −5,2 | **−3,5** | −2,1 | −1,1 | −0,3 | 0,0 | **0,0** | **0,0** |

**El −3 dB cae en ~70 Hz. En 723 Hz la respuesta es plana.** No hay ningún
pasaalto de 723 Hz en el camino real.

#### El mecanismo, y de paso cierra la ganancia (§20)

§19.3 conjeturaba que el PAM8302A tiene **resistencia de entrada interna en
serie** con la externa, y que la fórmula del datasheet pide el total. La
aritmética lo confirma: **un solo valor de ~9-10 kΩ interna explica las DOS
mediciones a la vez.**

| R interna | R total | pasaalto | ganancia |
|---|---|---|---|
| 0 (lo que suponía §2.1) | 1 k | 723 Hz | +49,1 dB |
| **~9 k** | **10 k** | **72 Hz** ✅ | **+29,1 dB** ✅ |
| medido | | **68 Hz** | +17 a +29 dB (sin cerrar) |

> **Y esto cierra la duda de §20.** Si la ganancia fuera **+17 dB**, la R total
> tendría que ser 40 kΩ, y entonces el pasaalto caería en **18 Hz** — contra los
> 68 Hz medidos. Las dos cosas salen de la MISMA R total, así que no pueden
> discrepar: **la medición del pasaalto corrobora +29 dB y descarta +17 dB.**

#### Qué compraría el cambio realmente

Con R interna ≈ 9 kΩ, subir la externa de 1 k a 20 k lleva el total de 10 k a
30 k, no de 1 k a 20 k:

| | ganancia | pasaalto | pasabajos (con C 4,7 nF) |
|---|---|---|---|
| hoy | +28,2 dB | 66 Hz | 33,9 kHz |
| R_ext 20 k | +19,5 dB | 24 Hz | **1,69 kHz** 🔴 |

- **Gana 8,7 dB de margen digital, no 26.**
- **Mejora un pasaalto que ya es irrelevante**: 68 Hz está muy por debajo de la
  resonancia de 125-180 Hz de los parlantes (`loopback_debug.md` §28). La banda
  grave la limita el driver, no el amplificador.
- **Y sigue en pie la trampa del filtro**: R_ext forma el pasabajos de
  reconstrucción con el capacitor de 4,7 nF a masa. Subirla sin bajar ese
  capacitor a 220 pF deja el pasabajos en 1,7 kHz y se lleva los agudos.

#### Veredicto

**No hacerlo.** Se cambian 128 componentes y se altera el sonido de forma
audible —8,7 dB de ganancia y otra respuesta en graves— a cambio de un margen
que §19.4 ya dice que sobra: *"con +29 dB sobran ~20 dB de margen; el equipo
viene corriendo muy por debajo de lo que puede"*. Y **crea una diferencia de
timbre entre las DS11 y las DS12** en una instalación pensada para que 15 nodos
suenen igual.

Si en algún momento hiciera falta más margen digital, el camino barato es el
volumen maestro del PCM1690, que ya es ajustable y no cuesta un rediseño.

**Lo único que queda vivo de este ítem**: los designadores, por si alguna vez se
tocan — **R_I = R38 … R101** (1 kΩ, 64 unidades) y los shunt de 4,7 nF C0G (64).

> **Lección de método, que es lo que hay que llevarse:** §2.1 quedó marcada como
> corregida en su propio archivo, pero este documento se había escrito desde
> ella y nadie propagó la corrección. Un número derivado del esquemático
> sobrevivió tres semanas a la medición que lo desmentía. **Al citar un número
> de `loopback_debug.md`, chequear si tiene una sección de corrección más
> abajo.**

### 0.8. Puerto USB del STM32 — es una decisión de encapsulado, no de firmware

**Abierto 2026-09-03.** Objetivo declarado: que la placa sea una interfaz de audio de
**8 entradas / 8 salidas** controlable por USB. Análisis completo en
**[DS12_USB_STM32.md](DS12_USB_STM32.md)**. Lo esencial:

- **PA11 y PA12 (los pines de USB) están físicamente sin conectar en la DS11** ✅
  (verificado: son nets de un solo nodo en `DS.txt`). PA9 y PA10 también. Un puerto USB
  Full-Speed **no cuesta ninguna migración de pines**.
- **Pero 8 ch × 24 bit × 48 kHz = 1152 bytes por trama, y el máximo de un endpoint
  isócrono Full-Speed son 1023.** No entra ni en una sola dirección. Es un techo del
  estándar USB 2.0. El techo práctico de FS es **4 in / 4 out a 24 bit/48 kHz**.
- **8×8 exige High-Speed**, que en el H743 exige un **PHY ULPI externo**, y ULPI exige
  `ULPI_DIR` y `ULPI_NXT`, que **no existen en LQFP100 ni en LQFP144** ✅ (verificado en la
  tabla 9 del datasheet). Hace falta **STM32H743IIT6, LQFP176**.
- Recomendación: **DS12 = arreglo de alimentación + USB Full-Speed** (que además trae DFU
  por cable, consola CDC y control MIDI, todo gratis en pines muertos), y **la migración a
  LQFP176 como decisión separada**, tomada una vez que la pila UAC2 esté validada en FS.
  Meter un cambio de encapsulado del MCU en la misma tirada que el arreglo de alimentación
  convierte una revisión acotada en un bring-up completo.

---


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
> **🔧 2026-09-11 — este tap NO cierra el lazo DAC→ADC como está planteado.** Las 8 entradas
> del AK5538 **ya están ocupadas por J9** (verificado: los 16 pines AINnP/N van todos a
> `/ADC_AK5538/J9_*`). No hay entrada libre donde inyectar. **Llevarlo a un ADC del STM32**:
> PC0 (U7 pin 15, ADC123_INP10) está sin conectar. ≈US$0,02 en pasivos y queda además
> independiente de SAI3_B. Ver [DS12_AUDITORIA.md](DS12_AUDITORIA.md) §4.

- Header 2 pines **DNP** desde VOUT ch0 del PCM1690 (antes del PAM) vía **R serie 1 kΩ** + GND.
- Habilita: self-test de toda la cadena DAC→ADC (chirp + correlación del AUTOFOCUS),
  verificación empírica de la latencia del nodo, y el **"oscilador en la cabecera"**
  (un nodo genera contenido y lo inyecta al daisy-chain analógico → los demás lo
  beamformean coherente). Ver [../../docs/multinode.md](../../docs/multinode.md) §2.
- Costo: 2 pads + 1 R.

### 2b. J7 pin 1 (3V3 del ST-Link) sin protección — ⚠️ el Schottky es el arreglo equivocado
Detectado en el bring-up de la DS11 (2026-08). El **pin 1 de J7 va directo al riel
`+3V3`**, mientras que el pin 10 (5V) sí pasa por el Schottky **D11**. Sin esa
protección el ST-Link intenta alimentar toda la placa (STM32H743 + 4 PCM1690 + AK5538
+ ESP32 ≈ 700 mA), no puede, y se apaga su LED — que es exactamente el síntoma que
hizo perder horas creyendo que había un corto. Mitigación en DS11: **cortar el pin 1
del cable**. Detalle en [../../docs/ds11_errata.md](../../docs/ds11_errata.md).

#### Corrección 2026-09-02: va una resistencia, no un diodo

Este ítem decía "Schottky en serie, igual que el D11 del pin 10". **Un Schottky
orientado sonda→placa arregla el backfeed pero deja intacto el modo de falla que
efectivamente ocurrió**: sigue permitiendo que el ST-Link intente alimentar los 700 mA
de la placa. Es el diodo el que le abre esa puerta.

El pin 1 de un conector de debug **no es un riel de alimentación: es `VTref`**, una
entrada de sensado con la que la sonda averigua a qué tensión trabaja el target y ajusta
sus level shifters. En un ST-LINK/V2 o V3 originales es una entrada de alta impedancia
(no puede alimentar nada); en los dongles clon ese pin es la salida de un AMS1117 y sí
entrega corriente. **Como el comportamiento depende de qué sonda se enchufe, la
protección tiene que estar en la placa.**

- **DS12: `+3V3 ─ 100 Ω ─ J7.1`.** La sonda sensa 3,3 V con error despreciable y la
  corriente de falla queda limitada a ~33 mA en cualquiera de las dos direcciones.
  (1 kΩ es aún más seguro, pero algunas sondas consumen unos mA en VTref y ahí 1 kΩ
  ya introduce error de sensado. 100 Ω es el compromiso.)
- Opcionalmente un 0 Ω **DNP** en paralelo, serigrafiado, por si alguna vez se quiere
  el modo "alimentar desde la sonda" a sabiendas.
- **D11 (pin 10, 5 V) se queda como está**: ese sí es un camino de alimentación
  legítimo y su Schottky ya impide el backfeed hacia el VBUS del host.

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

### 3. Línea de timing GPIO4→~~PC13~~ (ESP32→STM32) — 🔴 **CAMBIAR DE PIN**

> **🔧 2026-09-11 — PC13 NO SIRVE.** La errata de silicio **ES0392 §2.2.30** dice que en
> encapsulado LQFP, conmutar PC13 **corrompe el oscilador LSE**, y el workaround declarado
> es **"ninguno"**. Elegir otro GPIO libre (hay 25; ver [DS12_AUDITORIA.md](DS12_AUDITORIA.md)).
> Este ítem se escribió antes de cruzar el diseño contra la errata.

Hedge opcional analizado el 2026-07-07 (verificado en netlist). No entró en DS11.
Para DS12, si se quiere un disparo de timing sin jitter entre procesadores:
GPIO4 (U5 pin 18) → PC13 (U7 pad 7) con **R serie ~220 Ω** cerca del ESP32; PC13 como
entrada/EXTI con pull interno (activa-baja). **No rutear la inversa** — el UART ya es
interrupt-driven en ambos sentidos. Ver [../../docs/esp32_roles.md](../../docs/esp32_roles.md).

### 4. Ferrites de salida de los PAM (64 posiciones) — solo si se quiere full power
Hoy: un solo bead genérico (C21286, "FerriteBead" 0805) para las 67 posiciones,
incluidas las 64 de salida de parlante (2 por parlante) + FB2/FB3 de potencia del +5VA.
Para el uso real (muy por debajo del full output) **está bien por corriente** — es chica y no
satura. 🔧 **Pero "filtra bien" es falso**: el datasheet del PAM8302A dice que el filtro de
ferrite actúa *"de ~1 MHz para arriba"* y la portadora está en **200-300 kHz**, así que la
fundamental no la atenúa nadie, a ninguna longitud de cable. Importa más ahora que el arnés
se alarga 1,63× con el layout 2×16 (§0.8.1). **Solo si DS12 va a correr a plena potencia**: especificar para
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
- Sustituciones de BOM validadas **eléctricamente** (⚠️ validado ≠ disponible, ver abajo):
  100pF→C327198 (YAGEO NP0 ±1%), 4.7nF→C2961271 (**C0G confirmado por Fernando**),
  RN1-4→16×75Ω discretas (R133-148, R131=33Ω intacto en MCLK), electrolíticos en rieles ≤5V
  salvo C56 (50V en +24V), divisor buck 40.2k/7.5k.

  > **🔴 2026-09-11 — el 4,7 nF `C2961271` tiene 115 unidades y la tirada de 10 necesita 640.**
  > Reemplazo: **`C85980`** (Murata GRM1885C1H472JA01D, 4,7 nF 50 V C0G ±5% 0603, 49.830 en
  > stock) o `C576818` como segunda fuente. Y una regla que sale de esto: **el stock se verifica
  > contra `jlcpcb/production_files/BOM-DS.csv`, no contra el netlist** — `DS.txt` todavía dice
  > `C883564` para ese mismo capacitor. Ver [DS12_AUDITORIA.md](DS12_AUDITORIA.md) §1.

  > **🔧 Las 16 resistencias de 75 Ω de MCLK son el valor equivocado** para terminación de
  > fuente, y además dejan la red asimétrica (75 Ω a los DAC, 33 Ω al ADC). Unificar en **33 Ω**
  > y —lo que más importa— ponerlas **junto al STM32**, no junto al DAC: hoy están a 40 mm de la
  > fuente, así que no terminan nada.
- D8 = SMBJ24A unidireccional legítimo (datasheet verificado).
- Rotation corrections aplicadas: JST_SH_BM10B=180, AP2125/SOT-23-5, PRTR/SOT-143=90,
  220µF/CP_Elec_6.3x7.7=180.

---

## 0.8 — 2 filas × 16 columnas entrelazadas: la disposición que pide la obra

**Propuesta de Fernando, 3-sep-2026, después de escuchar el experimento
vertical.** Es correcta, y el motivo de fondo merece escribirse porque invierte
la intuición.

### El requisito, dicho como requisito

En una instalación de Domosónica la gente **camina por el piso** y tiene
distintas estaturas. Lo que hay que conseguir es un foco angosto en el plano
horizontal —para que se encuentre caminando— y **extendido en vertical**, para
que una persona de 1,60 y otra de 1,90 sintonicen el mismo punto sin agacharse.

### Por qué eso invierte el diseño actual

El ancho del haz en un eje es **inversamente proporcional a la apertura en ese
eje**. La DS11 tiene 480 mm de apertura vertical, y por eso enfoca en vertical —
que es justo lo que **no** hace falta. Para que no enfoque en vertical hay que
hacer el array **bajo**, no alto.

Y lo entrelazado es el hallazgo fino: con las dos filas corridas 40 mm, el paso
**efectivo** en X pasa de 80 a 40 mm y el techo de lóbulos de rejilla se
duplica.

| disposición | apertura X | apertura Y | techo sin aliasing | haz X @1k | haz X @2k | haz X @4k | haz Y @2k |
|---|---|---|---|---|---|---|---|
| DS11 actual 8×4 | 560 mm | 480 mm | 2144 Hz | ±306 | ±153 | ±77 | ±179 |
| **2×16 entrelazado, 80 mm** | **1240 mm** | 160 mm | **4288 Hz** | **±138** | **±69** | **±35** | **±536** |
| 2×16 entrelazado, 60 mm | 930 mm | 120 mm | 5717 Hz | ±184 | ±92 | ±46 | ±715 |
| 1×32, 40 mm | 1240 mm | 0 | 4288 Hz | ±138 | ±69 | ±35 | omni |

(haz = medio ancho a −6 dB, a 1 m)

**Lo que gana el 2×16 entrelazado sobre lo que hay:** el doble de resolución
horizontal, el doble de banda utilizable, y ±536 mm de cobertura vertical a 1 m
en vez de ±179. **Sin tocar la electrónica**: mismos 32 canales, mismos
amplificadores, mismos DAC. Cambia el layout mecánico y el arnés.

### La tensión que hay que resolver antes de comprar nada

- Paso chico en X (resolución y anti-aliasing) exige conos **angostos**.
- Cobertura vertical amplia exige conos **bajos**.
- Las dos cosas juntas son un cono **chico**, que da menos nivel y menos grave.

**Sobre «parlantes longitudinales»:** un cono alargado en **vertical** da paso
chico en X (bueno) pero **angosta** la dispersión vertical, que es exactamente
lo que no se quiere. Uno alargado en horizontal da buena dispersión vertical
pero impide juntar las columnas. Conviene elegir el driver con la cuenta de
arriba delante.

### Lo que esto le hace al issue #15

Lo **retira**. El experimento del barrido vertical existía para decidir si había
que bajar el paso de filas de 160 mm. Con 2 filas no hay steering vertical que
aliasear: la pregunta desaparece en vez de contestarse. Verificado a oído el
3-sep-2026 que el movimiento vertical **sí se percibe** en la DS11 — pero ya no
es lo que se quiere.

---

### 0.8.1 — Verificación del "sin tocar la electrónica" (2026-09-10)

Auditoría de la dimensión `system` sobre el netlist de producción, el PCB y el firmware.
**La afirmación es correcta para el hardware. NO lo es para el firmware, y el modo de falla
es silencioso.**

#### 🔴 El buffer de retardos no alcanza — y recorta sin avisar

`main.c:94` define `TX_MAX_DELAY_SAMPLES 128`, y el comentario inmediatamente anterior
**deriva ese número de la geometría actual**: *"diagonal ≈ 768mm ≈ 108 samples @48kHz […]
so 128 covers every possible focus"*. Con el array nuevo esa premisa deja de valer:

| disposición | diagonal | muestras @48 kHz | ¿entra en 128? |
|---|---|---|---|
| DS11 8×4 (600×480) | 768 mm | 107,5 | ✅ |
| **DS12 16×2 (1240×160)** | **1250 mm** | **175,0** | ❌ |
| tira 32×1 @60 mm (1860×0) | 1860 mm | 260,3 | ❌ |

`Beam_SetTxFocus` (`main.c:4780`) recorta en silencio:

```c
if (dq > (TX_MAX_DELAY_SAMPLES << 8)) dq = TX_MAX_DELAY_SAMPLES << 8;
```

Los parlantes de los extremos reciben menos retardo del que les corresponde: **el haz se
forma igual, pero mal, y nada lo delata** — el mismo modo de falla del arnés cruzado.

**Arreglo: `TX_MAX_DELAY_SAMPLES` 128 → 192.** El propio comentario fija la cota superior en
544 (`delay + 480 ≤ 1024`), así que hay lugar de sobra y no cuesta memoria.

#### ✅ El cambio de geometría en sí son dos líneas

```c
#define SPK_ROWS  4  →  2
#define SPK_COLS  8  →  16
```

**El entrelazado ya está implementado**: `SPK_ROW_DX_MM = 40.0f` corre las filas impares
40 mm, que es exactamente el paso efectivo que este §0.8 quiere conseguir. No hay que
escribir lógica nueva.

Arrastra dos cosas menores: el banner de `Beam_InitTxGeometry` tiene `7 *` y `3 *`
hardcodeados (`COLS−1` y `ROWS−1`), y **`SWEEP_Y_MM = 600` queda mal** — su comentario lo
justifica con *"la apertura en Y son 480 mm"*, y pasan a ser 160.

#### 🟡 El arnés se alarga, y se suma con el ADC

| | apertura | cable más largo | cable medio | cobre total |
|---|---|---|---|---|
| DS11 8×4 | 600 × 480 mm | 384 mm | 242 mm | 7,8 m |
| DS12 16×2 | 1240 × 160 mm | **625 mm** (1,63×) | **336 mm** (1,39×) | **10,8 m** |

Precisión sobre el datasheet del PAM8302A: los 20 cm son la condición para la solución
*simple* (sólo bulk); para cables largos pide el bead, **y la placa ya tiene los 64**. No se
está violando nada. Lo que sí vale: *"the ferrite filter depresses EMI of around 1MHz and
higher"* mientras `fOSC` = **200-300 kHz** → la portadora no la atenúa nadie, a ninguna
longitud.

**El riesgo real es la suma con la dimensión `adc`**: 64 conductores sin blindaje, 1,63× más
largos, radiando 250 kHz, por la misma estructura que el arnés de micrófonos — que entra a un
ADC cuyo único polo está en 3,39 MHz. Ninguna de las dos dimensiones por separado lo veía.

Los 16 JSPK además salen de un racimo de **71,5 mm** (X≈206/220, Y 47,5→119,0) sobre el borde
derecho: el arnés abre en abanico desde un solo punto hacia 1240 mm.

#### 📌 La polaridad del conector de parlantes es `+ − − +` — anotarlo de una vez

Confirmado en **los 16** conectores. El mecanismo, porque es fácil leerlo mal:

```
Amp A1 (par, U12/U14/…):   DAC VOUTn− → IN+   y   pin 5 = AMP1_OUT−   ← doble inversión
Amp A2 (impar, U13/U15/…): DAC VOUTn+ → IN+   y   pin 5 = AMP2_OUT+   ← directo
```

Las dos inversiones de A1 **se cancelan**, así que la polaridad acústica es uniforme y
`speaker_sign[ch] = +1` para todos es correcto. En el conector queda espejado:
**pines 1 y 4 = positivo, pines 2 y 3 = negativo.**

⚠️ **Trampa de lectura**: el campo `pinfunction` del netlist dice `OUT+_5` / `OUT−_8` en los
32 amplificadores por igual, porque viene del símbolo. La polaridad real está en los
**nombres de red**, que sí se invierten entre A1 y A2. Mirar el campo equivocado da `− + − +`
y es una conclusión incorrecta.

#### 🔴 Las redes de parlante son anónimas — y por eso el cruce sobrevivió

Las 64 salidas están en redes autogeneradas (`Net-(JSPK01-Pin_1)`). **No hay ningún nombre
contra el cual un cruce sea visible**, ni en el netlist ni en una revisión de esquemático.
Es la razón estructural de que el swap JSPK09/10 llegara a fabricación.

**Para DS12: nombrar las 64 redes `SPK_CH00_P` / `SPK_CH00_N` …** Convierte toda esa clase de
error en algo que se detecta con un grep. Vale más que cualquier revisión visual.

#### Estado de los ítems relacionados

- **JSPK09/10 cruzados** (§2d): ✅ **resuelto en el diseño nuevo** (Fernando, 2026-09-10).
- **Orden 1-16 conectado como 16-1**: era un **error de conexionado circunstancial** de las
  pruebas con micrófono, no una característica del diseño.
- **`SPK_PAIR_SWAP = 1`**: es un parche por software para el arnés de la DS11.
  **Con el arnés corregido tiene que volver a 0** — hay A/B en caliente por `CMD_SET_SPKMAP`
  y el banner del arranque lo declara, así que la vuelta es verificable sin instrumental.
