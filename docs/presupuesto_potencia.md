# DS11 — Presupuesto de potencia

> Armado el 2026-08-16 leyendo los datasheets del MP2315S, PAM8302A y PCM1690.
> Responde una pregunta que veníamos arrastrando sin números: **¿cuánta potencia
> hay realmente disponible para sonido, y cuándo se acaba?**
>
> Resumen: el buck da **15,45 W**, el consumo estático se lleva **~4-5 W antes de
> que salga un solo sonido**, y lo que queda limita la salida sostenida a
> **~0,3 W por canal** — unos 9 dB por debajo de lo que cada amplificador puede dar.

---

## 1. La fuente: MP2315S

| parámetro | valor | fuente |
|---|---|---|
| Rango de entrada **recomendado** | **4,5 – 24 V** | Recommended Operating Conditions |
| Máximo absoluto de VIN | 26 V | Absolute Maximum Ratings |
| Corriente continua | **3 A** | portada |
| Límite de corriente de pico | **4,5 / 5,5 / 6,5 A** (min/típ/máx) | Electrical Characteristics |
| Protección de sobrecorriente | **OCP con hiccup** | Features |
| Soft-start | 0,8 / 1,5 / 2,2 ms | Electrical Characteristics |
| Shutdown térmico | 150 °C, histéresis 20 °C | Electrical Characteristics |
| Disipación continua @ TA=25 °C | **1,25 W** | Absolute Maximum Ratings |
| θJA | 100 °C/W (PCB 4 capas) | Thermal Resistance |

**Salida útil: 5,15 V × 3 A = 15,45 W** para *toda* la placa.

### ⚠️ CORRECCIÓN (2026-08-16): 12 V sirve en banco, NO en daisy-chain

> La recomendación de abajo se escribió mirando el buck **en aislamiento** e ignoró
> la arquitectura del sistema. **Fernando señaló el problema: con distribución en
> daisy-chain, bajar la tensión obliga a cables mucho más gruesos.** Tiene razón, y
> los números lo confirman.

A igual potencia, la corriente escala inverso a la tensión, y las pérdidas en el
cable van con **I²R**. Para una cadena de 4 placas a plena carga (~17 W de entrada
c/u) sobre 10 m de cable de 1,5 mm² (0,23 Ω ida y vuelta):

| | corriente | caída | **pérdida en el cable** |
|---|---|---|---|
| **20 V** | 3,4 A | 0,79 V (4 %) | **2,7 W** |
| **12 V** | 5,7 A | 1,33 V (11 %) | **7,6 W** |

El buck ahorra ~0,3-0,6 W por placa al bajar de 19 V a 12 V — entre 1,2 y 2,4 W en
las cuatro. **El cable pierde 4,9 W más.** O sea que a 12 V se pierde más en el
cable de lo que se ahorra en los conversores: el balance da **negativo**.

Es el mismo principio por el que las redes eléctricas distribuyen en alta tensión y
bajan cerca del consumo. **Tensión alta para transportar, baja para convertir.**

**Recomendación corregida:**

| topología | tensión | por qué |
|---|---|---|
| **Una placa en banco**, cable corto | **12 V** | menos calor en el buck, más margen al máximo absoluto, cable irrelevante |
| **Daisy-chain de varias placas** | **19-20 V** | el ahorro del buck no compensa el I²R del cable |

Y el calor del buck a 19 V se relativiza: sólo supera el límite de disipación cerca
de **2 A de salida**, y por el presupuesto de la sección 3 tampoco podemos sostener
esa carga. **El problema térmico se auto-limita** por el mismo techo de potencia.

Para DS12 esto refuerza el ítem del buck de mayor capacidad: uno más eficiente a
entrada alta resuelve las dos cosas a la vez, sin obligar a elegir.

### Conviene bajar la tensión de entrada, no sólo alejarse de los 24 V (⚠️ ver corrección arriba)

La eficiencia del buck **empeora cuanto más alta es la entrada** (gráficos de
Efficiency vs Output Current, VOUT = 5 V):

| VIN | eficiencia @ ~2 A | pérdidas con 10,3 W de salida |
|---|---|---|
| 6,5 V | ~95 % | 0,54 W |
| **12 V** | **~92 %** | **0,90 W** |
| 19 V | ~87 % | 1,54 W |

Veníamos diciendo "alimentar a 19-20 V en vez de 24 V" por el margen contra el
máximo absoluto. Pero **12 V es estrictamente mejor que 19-20 V**: mismo margen de
sobra contra los 26 V, y **~40 % menos de calor en el buck**.

Eso importa porque la disipación continua permitida a 25 °C de ambiente es
**1,25 W**, y a 19 V con 2 A de carga ya estamos en ~1,54 W — *por encima del
límite*. A 12 V, la misma carga disipa 0,90 W y queda dentro.

En la placa nada más usa la tensión de entrada: el único consumidor es el buck (de
ahí salen +5VA para los amps, y por FB2+D9 el +5V que alimenta los reguladores de
3,3 V). **Bajar la entrada no tiene contraindicación conocida** — hay que probarlo
en banco, pero el análisis dice que sólo hay upside.

> ⚠️ Ojo: el TVS D8 (SMBJ24A) sigue sin proteger al buck a ninguna tensión, porque
> su VBR mínimo (26,7 V) está por encima del máximo absoluto del MP2315S (26 V).
> Eso no lo arregla bajar la entrada; está anotado para DS12.

### El hiccup explica la firma de los reinicios

El OCP del MP2315S es de tipo **hiccup**: ante sobrecorriente apaga y reintenta
periódicamente. Si el riel de 5 V colapsa y vuelve en ciclos, **esa es la firma**,
y se distingue de un brown-out por caída de la fuente externa. Vale tenerlo
presente: el loop de reinicios del 2026-08-04 era el límite de corriente de la
fuente de banco, pero un cuadro parecido puede venir del hiccup del propio buck.

---

## 2. El consumo estático — lo que se va antes de sonar

| consumidor | cantidad | consumo | total |
|---|---|---|---|
| **PCM1690** (DAC) | 4 | 558 mW típ / 847 mW máx c/u | **2,23 W** típ / 3,39 W máx |
| **PAM8302A** en reposo | 32 | 4 mA típ / 8 mA máx c/u @5,15 V | **0,66 W** típ / 1,32 W máx |
| **AK5538** (ADC) | 1 | 182 mW | 0,18 W |
| STM32H743 @480 MHz | 1 | ~0,5-1 W (estimado) | ~0,7 W |
| ESP32-C3 con WiFi | 1 | ~0,3-0,5 W medio | ~0,4 W |
| | | **TOTAL** | **≈ 4,2 W típ, hasta ~6 W** |

> **Los 4 PCM1690 son el mayor consumidor estático de la placa** — más que los 32
> amplificadores en reposo. 2,23 W típicos, que son el **14 % del presupuesto total**
> sin producir ningún sonido.

**Queda para audio: 15,45 − 4,2 ≈ 11 W** (y en el peor caso de dispersión, ~9 W).

---

## 3. Cuánta salida sostenida permite eso

Con ~11 W disponibles y 85 % de eficiencia de los PAM:

> **≈ 9,4 W acústicos repartidos en 32 canales = 0,29 W por canal.**

Contra los **2,5 W** que cada PAM8302A da con 4 Ω a 5 V, eso es **−9,3 dB**.

| escenario | potencia por canal | ¿entra? |
|---|---|---|
| Los 32 a máxima potencia | 2,5 W → **94 W de entrada** | ❌ 6× el buck |
| Los 32 a −9 dB de su máximo | 0,29 W → ~11 W | ✅ justo en el límite |
| **Volumen −20 dB** (uso normal) | ~25 mW → 0,8 W | ✅ sobradísimo |
| **Volumen −40 dB** (pruebas) | despreciable | ✅ |

**Conclusión práctica: a los volúmenes de trabajo el consumo es un no-problema.**
El presupuesto sólo se vuelve crítico acercándose a plena potencia — y ahí no
entra ni cerca.

### ⚠️ Por qué el beamforming es el peor caso

Un haz enfocado hace que **los 32 canales reproduzcan la misma señal en fase**.
No hay promediado estadístico entre canales: **todos pican juntos**. Cualquier
cálculo que asuma que las demandas se reparten en el tiempo no aplica acá.

### El bulk no es lo que salva los picos

> ⚠️ **Corrección (2026-08-17)**: el bulk de `+5VA` son **cinco** electrolíticos de
> 220 µF, no cuatro — además de C290-C293 está **C71** (verificado en el netlist y
> en el BOM: los 5 comparten el part number C25503905). O sea eran **1100 µF** y
> tras remover C290 quedan **880 µF**, no 660. `ds11_errata.md` también lo dice mal.

Con **880 µF**, la energía disponible antes de caer de 5,15 V a 4,5 V es:

`E = ½·C·(V₁²−V₂²) = ½ · 880 µF · (5,15² − 4,5²) ≈ 2,8 mJ`

Ante un déficit de 15 W, eso alcanza para **≈ 190 µs**. Un pico musical dura
milisegundos, o sea **dos órdenes de magnitud más**.

> **El bulk filtra el rizado de conmutación, no sostiene picos musicales.** Lo que
> realmente da margen ante un transitorio es el **límite de corriente de pico del
> buck (4,5-6,5 A ≈ 23-33 W)**, que es varias veces la corriente continua.
>
> Corolario tranquilizador: **remover C290 (−25 % de bulk) no es tan grave como
> parecía** — pasó de 184 µs a 140 µs de reserva, y en ninguno de los dos casos
> el bulk era el mecanismo de protección. Repoblarlo sigue siendo deseable, pero
> no es el factor que decide si hay brown-out.

---

## 3bis. Tres hallazgos posteriores que cambian el cuadro

> Salieron de una revisión cruzada del netlist contra los datasheets (2026-08-17).
> Los cuatro puntos de abajo están **verificados contra `hardware/DS/DS.txt` y el
> BOM de producción**, no inferidos.

### 🔴 A. Los amplificadores están limitados térmicamente ANTES que el buck

El encapsulado en el netlist es **`Package_SO:MSOP-8_3x3mm_P0.65mm`**, y el
datasheet del PAM8302A da **θJA = 180 °C/W para MSOP-8** (contra 115 para SO-8 y
47,9 para U-DFN3030). Con OTP a 120 °C mínimo y 40 °C de ambiente dentro de una caja:

`P_disipable = (120 − 40) / 180 = 0,44 W por amplificador`

Un PAM8302A dando 2,5 W sobre 4 Ω con 85 % de eficiencia disipa **exactamente
0,44 W**. O sea: **un solo amplificador a potencia plena ya está en el límite de su
protección térmica — aislado — y hay 32 calentándose entre sí.**

> **Esto invalida la idea de que un buck más grande habilite la potencia plena**
> (§5.1 de este documento decía eso). El cuello de botella no es la fuente: son los
> amplificadores. Habilitar potencia plena exigiría cambiar el encapsulado a
> `PAM8302AAYCR` (U-DFN3030), con pad térmico y vías bajo cada uno de los 32.

Nota operativa: **el peor caso térmico no es la música, es el tono sostenido.** El
channel walker y el chirp del AUTOFOCUS son senoidales en fase en todos los canales,
con crest factor 0 dB. Los procedimientos de test son la condición más dura que ve
la placa.

### 🔴 B. El riel digital cuelga del de amplificadores por un bead y un Schottky

Verificado en el netlist, la cadena es:

```
+5VA (5,15 V) → FB2 → D9 (SS34) → +5V → U11 (AP1117-33) → +3V3
                                       → U10 (AP2125)   → +3,3VA (ADC)
```

**FB2 es el mismo bead genérico que las 64 salidas de parlante** (mismo part number
en el BOM), pero acá lleva la corriente completa del riel digital + analógico. Entre
su DCR y el Vf del SS34 se van **~0,5 V**, dejando al AP1117-33 con apenas
**~250-350 mV** sobre su dropout antes de perder regulación.

Dos consecuencias:

1. **Explica por qué repoblar C290 no iba a arreglar el brown-out**: no es un
   problema de energía almacenada, es de **margen de dropout en serie**. El bulk
   está del lado equivocado de esos 0,5 V.
2. **El +3,3VA del ADC** —sus AVDD y sus cuatro VREFH— está aguas abajo de 32
   amplificadores class-D, filtrado sólo por ese bead saturado.

**Prueba de banco que lo cierra en una tarde**: puentear FB2 y D9 con hilo (con
ST-Link y USB desconectados, porque D9 es lo que impide el backfeed) y repetir el
walker a volumen. Si desaparecen los reinicios, diagnóstico confirmado.

### 🟡 C. El buck tiene el modo de ahorro por salteo de pulsos ACTIVADO

R35 (100 k, VCC→AAM) y R36 (9,09 k, AAM→GND) fijan V_AAM ≈ 0,47 V. El datasheet del
MP2315S es explícito: *"Tie AAM to VCC or leave AAM floating to disable AAM mode"*.

O sea que a carga liviana —la condición normal de esta placa— el conversor entra en
**burst**, y la frecuencia de repetición del burst **cae en banda audible y varía con
la carga**. Ese rizado va al riel de los 32 amps (PSRR del PAM8302A = 50 dB @1 kHz,
peor arriba) y, por la cadena de B, al riel analógico del ADC.

**Se corrige hoy en DS11 removiendo un solo componente: R36.** Eso deja AAM tirado a
VCC por R35 → PWM forzado a 500 kHz fijo, fuera de banda. Cuesta ~0,2-0,4 W más de
disipación a carga liviana; para una placa de audio es un intercambio obvio, y es
reversible.

### ✅ D. Lo que sí está bien: VCAP

Table 114 del datasheet (Rev 11) pide **2,2 µF con ESR < 100 mΩ** en cada pin VCAP,
y admite dos capacitores. La placa tiene **C37 y C39 = 2,2 µF 0603** (LCSC C23630) en
U7.48 y U7.73. **Correcto.** Único matiz para DS12: verificar el dieléctrico, porque
el datasheet sólo tolera ±20 % y un X5R chico puede perder más que eso por DC bias.

### ⚠️ E. A 480 MHz el chip tiene 105 °C de T_J máxima, no 125

Table 113 del datasheet:

| Power scale | Max T_J | Max frecuencia |
|---|---|---|
| **VOS0** | **105 °C** | 480 MHz |
| VOS1 | 125 °C | 400 MHz |

Correr a 480 MHz **cuesta 20 °C de margen térmico**. Y estamos en el máximo
simultáneo de las tres: fCPU 480, HCLK 240, PCLK 120 MHz. El soak midió 38-40 °C de
juntura, así que hay 65 °C de sobra — pero **eso fue al aire libre**. En una caja
cerrada, con los 32 amplificadores adentro, el margen es lo primero que se gasta.

---

## 4. Verificaciones pendientes en banco

- [ ] **Medir el consumo real** en reposo (amps ON, silencio) y compararlo con los
      4,2 W estimados. Es la validación de toda esta tabla.
- [ ] **Probar a 12 V** de entrada y comparar temperatura del buck contra 19-20 V.
- [ ] **Temperatura del MP2315S** con los 32 canales sonando fuerte un rato largo.
      Si llega a shutdown térmico, la salida cae de golpe y vuelve — firma distinta
      a la del hiccup por sobrecorriente.
- [ ] **Verificar el encapsulado del PAM8302A en el BOM**: θJA va de **47,9 °C/W**
      (U-DFN3030) a **180 °C/W** (MSOP-8) según el package. Con 32 unidades, esa
      diferencia decide si el calor es un tema o no.

---

## 5. Para DS12

1. **Buck de mayor capacidad** — ya estaba anotado por el tema de la tensión, pero
   ahora hay un segundo motivo: 3 A es poco para 32 amplificadores más 4 DACs. Un
   buck de 5-6 A daría margen real para usar la potencia que los PAM pueden dar.
2. **Fijar la tensión de sistema en 12 V** en vez de 19-20 V (más eficiencia, menos
   calor, más margen) y elegir el TVS en consecuencia (SMBJ13A o similar).
3. Si se quiere plena potencia, considerar **alimentar los amps con su propio
   riel** separado del digital, para que un brown-out de audio no arrastre al MCU.

---

## §D — El hundimiento del riel de 3V3, MEDIDO (3 de septiembre de 2026)

El §B describía como hipótesis razonable que el riel de +3V3 se hunde al
encender los amplificadores y que IC1 (MAX809T, umbral 3,08 V) reinicia el
STM32. **Ahora está medido, desde adentro de la placa y sin agregar un solo
componente.**

### Cómo se mide

El STM32H743 tiene un canal interno de ADC a su referencia de banda prohibida
(VREFINT ≈ 1,21 V, fija). Midiéndola contra un fondo de escala que *es* VDDA, el
cociente da VDDA, y la calibración de fábrica está grabada en la flash de
sistema:

```
VDDA [mV] = 3300 × VREFINT_CAL / lectura        VREFINT_CAL en 0x1FF1E860
```

`Rail_Poll()` en `main.c` lo hace sin bloquear (arranca la conversión en una
pasada del main loop y la cosecha en otra, mirando EOC) y guarda el **mínimo
retenido**. Eso último es lo que hace la diferencia: un valor instantáneo leído
una vez por segundo no ve un hundimiento de milisegundos.

### Lo medido

| condición | VDDA típico | mínimo retenido |
|---|---|---|
| amps apagados, salida en mute | 3300 mV | 3251–3255 mV |
| amps **encendidos**, salida en mute | 3300 mV | 3247–3254 mV |
| amps encendidos, **volumen −20 dB** | 3195–3221 mV | **3068 mV** |
| amps encendidos, **volumen −18 dB** | 3219 mV | **3075 mV** → **RESET** |

**Encender los amplificadores no cuesta casi nada** (3254 → 3247 mV). Lo que
hunde el riel es la **corriente de audio**: a −20 dB el mínimo cae 230 mV y toca
el umbral del supervisor; a −18 dB la placa **se reinicia**.

Y el reinicio se identifica sin ambigüedad: `/status` informa `reset: PIN`,
`rsr 00460000` — el pin **NRST**. No fue software (`SFT`), ni el brownout
interno del núcleo (`BOR`), ni el watchdog. Fue IC1.

### Precisión, dicha de entrada

La referencia interna tiene ±1 % de tolerancia y la calibración se tomó a 30 °C,
así que **el valor absoluto vale ±30 mV**. Lo que vale mucho más son los
**cambios** —la caída de 230 mV— porque el error es común a todas las lecturas.
Y de todos modos la evidencia dura del umbral no es el número: es el reinicio
con causa PIN.

### Qué significa

1. **El §B queda confirmado, y con el mecanismo identificado.** No es la fuente
   de banco, no es el consumo medio, no son los 24 V: es la caída del **3V3
   digital** durante los transitorios de corriente de audio, aguas abajo del
   cuello de botella FB2 → D9 → AP1117-33.
2. **El margen de operación es de ~200 mV y desaparece con audio fuerte.** Con
   el ST-Link puesto (que alimenta el riel por J7 pin 10, *después* del cuello
   de botella) la placa aguanta más; sin él, menos. Eso explica por qué las
   cinco DS11 nunca corrieron sin USB.
3. **El equipo dejó de ser ciego.** El amperímetro de la fuente no podía verlo:
   promedia, mide del lado de la entrada, y el evento son 230 mV durante
   milisegundos dos conversiones más adelante.

### Para la DS12

Esto le pone número al ítem §0.1 de `DS12_PLANNING.md`. El requisito no es
"mejor regulador": es que **el camino de 3V3 sostenga el transitorio**, lo que
apunta a más bulk local en el 3V3 y a sacar el diodo serie y el ferrite del
camino de alimentación principal. Y a **medir los otros rieles**, que hoy siguen
siendo invisibles: +5V, +5VA y +24V necesitan divisores resistivos a entradas de
ADC — el 3V3 salió gratis sólo porque el STM32 se mide a sí mismo.
