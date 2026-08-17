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

### 🔴 Conviene bajar la tensión de entrada, no sólo alejarse de los 24 V

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

Con **660 µF** (tras remover C290; eran 880 µF), la energía disponible antes de
caer de 5,15 V a 4,5 V es:

`E = ½·C·(V₁²−V₂²) = ½ · 660 µF · (5,15² − 4,5²) ≈ 2,1 mJ`

Ante un déficit de 15 W, eso alcanza para **≈ 140 µs**. Un pico musical dura
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
