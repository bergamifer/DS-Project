# DS12 — La entrada pasa de 20 V a 24 V

> Escrito el 2026-10-23. **Decisión de Fernando:** el sistema se alimenta con fuentes de 24 V
> porque el vatio sale mucho más barato que en los ladrillos de notebook de 19-20 V. Se
> mantienen los dos reguladores (AP63356 y AP63301) y se adaptan las protecciones.
>
> Este documento **reemplaza la decisión de 20 V** del §10 de
> [DS12_ALIMENTACION.md](DS12_ALIMENTACION.md). Lo demás de ese documento sigue valiendo.
>
> **No se toca el PCB**: todos los cambios son de part number sobre la misma huella. El
> dibujo lo hace Fernando.
>
> Cómo se verificó cada cosa:
> - netlist exportado del esquemático actual (19-oct) con `kicad-cli`;
> - hojas de datos en `.ds12_audit/hojas/`;
> - simulación con autoprueba:
>   [`revision_alimentacion/s13_entrada_24v.py`](revision_alimentacion/s13_entrada_24v.py);
> - las fugas de D23 con `a2_drenaje.py`, recalculadas a 24 V.
>
> Lo que **no** está verificado se marca ⚠️.

---

## 0. En una tabla

| ref | hoy | **con 24 V** | ¿obligatorio? | huella |
|---|---|---|---|---|
| **D9** (TVS) | SMBJ20A `C364296` | **SMBJ24A `C908801`** (el de la DS11, en `BOM-DS.csv`) | 🔴 **sí** | `D_SMB`, la misma |
| **F1** (fusible) | JFC1206-1200FS, 2 A `C136345` | **JFC1206-1300FS, 3 A `C136347`** | 🟡 sí | 1206, la misma |
| **Q1** (P-FET) | AO3401A, −30 V | queda. Opcional: Si2319CDS (−40 V, VGS ±20 V) | no | SOT-23 ⚠️ verificar pinout |
| net `+20V` | `+20V` | `+24V` (nombre, TP5, serigrafía) | cosmético | — |
| todo lo demás | | **sin cambios**: verificado pieza por pieza en el §4 | — | — |

**No cambia ningún conexionado.** No hay piezas nuevas, ni piezas que se van, ni pistas
distintas.

🔧 **Corrección de lo que dije en el chat:** había dicho que los electrolíticos C273 y C314
tenían que pasar de 35 V a 50 V. **No hace falta**, y conviene no hacerlo: un 100 µF de
50 V no entra en la huella de 6,3 × 7,7 mm. Las cuentas están en el §4.

---

## 1. Por qué el SMBJ20A tiene que salir sí o sí

Un TVS es un diodo que **no conduce nada** mientras la tensión está debajo de un umbral, y
que conduce todo lo que haga falta cuando se pasa. Con eso "recorta" los picos. La hoja da
tres números:

- **VRWM**: hasta acá, fuga de 1 µA o menos;
- **VBR**: el umbral, que tiene dispersión de fábrica;
- **VC**: a cuánto recorta con la corriente máxima de pulso (IPP).

Valores de la tabla de MDD (`SMBJ20A_MDD_C364296.txt`, misma familia):

| | VRWM | VBR (dispersión de fábrica) | VC @ IPP |
|---|---|---|---|
| SMBJ20A | 20 V | 22,2 – 24,5 V | 32,4 V @ 18,6 A |
| SMBJ24A | 24 V | 26,7 – 29,5 V | 38,9 V @ 15,5 A |

Con **24 V y el SMBJ20A puesto**, la fuente cae justo adentro de su dispersión de VBR:

- en unas placas el TVS no hace nada;
- en otras conduce amperes desde que se enchufa, se cocina y falla en corto (es el modo de
  falla normal de un TVS), y entonces se abre F1.

**Es una lotería por placa.** Por eso es el único cambio sin discusión: **no enchufar una
DS12 a 24 V hasta que D9 sea SMBJ24A.**

> Dato al margen: en el esquemático, el campo *descripción* de D9 ya dice
> «24V 26.7V 38.9V», que son los números del SMBJ24A. Quedó copiado de la DS11. El *value*
> y el LCSC dicen SMBJ20A. Con el cambio, por fin coinciden los tres.

---

## 2. Los límites reales

Lo que pasa según la tensión que entrega la fuente. Ya con D9 = SMBJ24A:

| fuente | qué pasa | por qué |
|---|---|---|
| **< 11,1 V** | amplificadores apagados; el micro sigue andando desde ~4 V | UVLO del buck A (R151/R156, sin cambios) |
| 11,1 – 14,3 V | los amplificadores se apagan si venían prendidos, pero no arrancan | histéresis del UVLO |
| **14,3 – 24,0 V** | ✅ funcionamiento normal | — |
| **24,0 V** | ✅ **el valor a ajustar** | VRWM del SMBJ24A: fuga ≤ 1 µA |
| 24,0 – 25,5 V | ✅ funciona; la fuga del TVS crece pero sigue en microamperes | debajo de VBR mín (26,7 V) con margen para el frío ⚠️ |
| **> 25,5 V** | ⛔ **no.** Con frío, algún TVS empieza a conducir | el VBR baja con la temperatura (~0,1 %/°C típico en esta familia; ⚠️ no está en la hoja de MDD) |
| 26,7 – 29,5 V | ⛔ lotería por placa: algunos TVS conducen amperes, se queman y abren F1 | dispersión de VBR |
| 29,5 – 32 V | ⛔ los TVS de **todas** las placas conducen, se queman y abren F1. **Los bucks sobreviven** | el TVS hace de "fusible de tensión" |
| > 35 V sostenido | 💀 bucks (si el TVS y F1 no actuaron antes) | máximo absoluto DC de AP63356 y AP63301 |
| **invertida, hasta −30 V** | ✅ no pasa nada, ni siquiera se quema F1 | Q1 se apaga |

**La regla de operación: la fuente ajustada a 24,0 V, y nunca por encima de 25,5 V.**

> ⚠️ **Las fuentes industriales de 24 V traen un potenciómetro de ajuste**, y su rango
> llega mucho más arriba. Las Mean Well LRS, según los revendedores, van de 21,6 a 28,8 V
> ([openelab](https://openelab.io/products/meanwell-lrs150-24-power-supply)). Con el
> potenciómetro al máximo, esa fuente está en la fila "lotería".
>
> **Antes de conectar una placa:**
> 1. medir la fuente **en vacío**, con multímetro;
> 2. ajustarla a 24,0 V;
> 3. trabar el potenciómetro con una gota de esmalte.

---

## 3. Enchufe en caliente: lo que simula `s13_entrada_24v.py`

### 3.1 El mecanismo

Conectar la ficha con la fuente ya prendida arma un circuito LC:

- el cable es una inductancia (~0,5–0,7 µH por metro de par);
- la placa es una capacidad.

Un LC sin pérdidas que recibe un escalón **oscila hasta el doble de la tensión** (la
autoprueba lo comprueba: 10 V → 20 V). Con 24 V serían 48 V.

Lo que lo impide son los dos electrolíticos C273 y C314. Su resistencia interna (ESR,
0,6 Ω) convierte la oscilación en calor. Por eso el documento de alimentación insiste en
no cambiarlos por unos de baja ESR.

El TVS es la segunda barrera, por si la primera no alcanza.

### 3.2 Resultados

Barrido:

- cable de 0,3 a 7 µH, y de 0,10 a 0,35 Ω (cable + contactos + F1 + Q1);
- cerámicos efectivos de 9 a 18 µF, porque los 10 µF de 1210 pierden capacidad con la
  tensión ⚠️ sin curva de Samsung;
- ESR de 0,3 a 0,6 Ω;
- bucks apagados, que es el peor caso;
- TVS en su VBR máximo, que es el peor recorte.

| caso | pico en VIN_LOCAL (lo que ven los bucks) | ¿conduce el TVS? |
|---|---|---|
| 24 V, electrolíticos sanos | **24,0 V**: sin sobrepico | no |
| 24 V, electrolíticos sanos, 7 µH y 0,10 Ω (≈10 m de cable con resistencia irreal) | 26,7 V | no |
| 24 V, ESR ×4 (frío o envejecidos) | ≤ 27,4 V | no |
| 24 V, ESR ×8 (electrolíticos muertos) | ≤ 30,0 V | apenas: 0,9 A, 0,1 mJ |
| 24 V, **sin electrolíticos** (no montados o abiertos) | ≤ **32,5 V** | sí: 5 A, 1,3 mJ (el SMB aguanta 600 W de pulso) |
| 26,4 V, ESR ×8 | ≤ 32,3 V | sí: 4,5 A, 1,3 mJ |

**Ningún caso pasa los 35 V continuos de los bucks**, ni siquiera sin los electrolíticos.
El SMBJ24A coordina bien. El recorte de 38,9 V de la hoja es con 15,5 A, y en un enchufe
en caliente el TVS no ve más de 5 A.

### 3.3 Lo que sí cambia: el fusible

Además de oscilar, cargar los ~255 µF de la entrada hace pasar un pulso de corriente por
F1. Lo que desgasta a un fusible es la **integral de I²** de ese pulso (I²t). Si llega a
su I²t de fusión, se abre; si queda cerca, se fatiga y se abre en alguna de las próximas
enchufadas.

La energía del pulso crece con el cuadrado de la tensión: de 20 a 24 V es **×1,44**.

| | I²t del pulso | F1 = 2 A (0,41 A²s) | **F1 = 3 A (1,39 A²s)** |
|---|---|---|---|
| 20 V, ladrillo con cable (0,35 Ω) / fuente de lab (0,10 Ω) | 0,11 / 0,25 A²s | 27 % / 60 % | — |
| **24 V**, ídem | **0,16 / 0,36 A²s** | 38 % / **87 %** ⛔ | **11 % / 26 %** ✅ |
| 25,5 V, fuente de lab | 0,41 A²s | **≈ 100 %**: se funde | 29 % |

Guía de Littelfuse para fatiga por pulsos (genérica, ⚠️ no es de JDT): 22 % del I²t de
fusión aguanta ~100 000 pulsos, 29 % ~10 000 y 38 % ~1 000.

- **Con 2 A**: algunos cientos de enchufadas con fuente de laboratorio, y la primera puede
  quemarlo.
- **Con 3 A**: decenas de miles.

**¿El de 3 A sigue protegiendo?** Lo que hay que cubrir:

- **Consumo normal:** la placa consume a lo sumo **~1,0 A a 24 V** (los dos bucks al
  máximo: ~24 W). El de 3 A no se abre.
- **Un corto en la entrada** (TVS en corto, un 1210 fisurado por flexión, un buck roto):
  - con una fuente que entregue ≥ 6 A (cualquiera que alimente una cadena), se abre en
    milisegundos;
  - con una fuente chica, de una sola placa, la que corta es la protección de sobrecorriente
    de la fuente. Por eso la fuente tiene que tener **sobrecorriente en modo *hiccup*** (se
    apaga y reintenta; casi todas las Mean Well lo hacen ⚠️ confirmar en la hoja de la que se
    compre).

Verificado en LCSC: `C136347` es JFC1206-1300FS, 3 A, 63 V, I²t 1,39, 50 A de capacidad de
corte, 1206.

### 3.4 Heredado, y un 20 % peor: el pico de corriente pasa por Q1

El pulso de enchufe tiene un pico de corriente, que dura unos pocos microsegundos, mientras
se cargan los cerámicos:

| | ladrillo con cable (0,35 Ω) | fuente de lab (0,10 Ω) |
|---|---|---|
| 20 V | 39 – 49 A | 72 – 113 A |
| 24 V | 47 – 59 A | 86 – 136 A |

Ese pico atraviesa Q1, y durante los primeros microsegundos lo hace **por su diodo
interno**, no por el canal:

- R149 tarda ~65 µs (100 kΩ × 645 pF de Ciss) en bajar la compuerta;
- mientras tanto la compuerta sigue a la fuente por capacidad, y el FET no prende.

El AO3401A especifica **27 A de pulso (IDM)** y no dice nada del diodo en pulsos.

- **Está fuera de hoja desde la versión de 20 V.** No lo crea este cambio.
- Pulsos de microsegundos así suelen sobrevivirse, pero ⚠️ **no está verificado**.
- **Se mide en el banco** (§7).
- La única corrección real sería un limitador de corriente de arranque en la entrada, y eso
  es PCB.
- **Lo que lo evita sin tocar nada:** conectar la ficha con la fuente **apagada**. Una
  fuente conmutada tarda decenas de milisegundos en subir, y el pico baja a ~0,2 A.

---

## 4. Pieza por pieza: todo lo que ve la tensión de entrada

Es la lista completa del netlist (`+20V`, `Net-(Q1-D)`, `+VIN_LOCAL`, `Net-(IC4-EN)`,
`Net-(D21-A)`, `/DS_Power/IN`).

Máximos usados:

- **régimen**: 25,5 V (24 V en `VIN_B`, después de D20);
- **transitorio**:
  - el del enchufe en caliente con los electrolíticos puestos: 27–30 V;
  - el peor modelado: 32,5 V;
  - el recorte del TVS a corriente plena: 38,9 V (sólo con una perturbación externa grande).

| ref | qué es | nominal | régimen | transitorio (enchufe / recorte del TVS a IPP) | veredicto |
|---|---|---|---|---|---|
| J11, J12 | Phoenix MSTBVA 2,5 | cientos de V | — | — | ✅ |
| TP5, TP7, TP9 | puntos de prueba | — | — | — | ✅ |
| **F1** | fusible 1206 | 63 V | — | — | 🟡 **a 3 A** (§3.3) |
| **Q1** | AO3401A | **−30 V** / VGS ±12 V | polaridad invertida: −24 a −25,5 V | inversión en caliente: hasta ~−48 V sobre 80 pF de Coss, **~25 nJ** | ✅ con margen chico. Opcional abajo |
| R149 | 100 k 0402, compuerta a GND | 50 V de trabajo | 14–15,5 V, 2 mW | 22 V | ✅ |
| D21 | zener BZT52C10, VGS de Q1 | 500 mW | 0,14 mA, 1,4 mW | 0,25 mA | ✅ sin cambios: limita VGS a ~10 V con cualquier entrada |
| **D9** | TVS | — | — | — | 🔴 **a SMBJ24A** (§1) |
| C273, C314 | 100 µF / 35 V electrolítico | 35 V | 25,5 V = 73 % | hasta 32,5 V con margen; 38,9 V cae dentro de la tensión de *surge* típica | ✅ **se quedan** (ver nota) |
| C316, C324, C326 | 10 µF / 50 V X7R 1210 | 50 V | 51 % | 65 % / 78 % | ✅ |
| C315 | 100 nF / 50 V 0603 | 50 V | 51 % | 65 % / 78 % | ✅ |
| R151 | 470 k 0603 (UVLO arriba) | 75 V de trabajo | 1,2 mW | — | ✅ |
| R156 | 40,2 k 0402 (UVLO abajo) | — | EN ≈ 2,1 V | EN ≈ 2,9 V | ✅ |
| **IC4** | AP63356 | 32 V operación · 35 V DC · 40 V 400 ms | 25,5 V | ≤ 32,5 V / 38,9 V (< 40 V por 400 ms) | ✅ |
| IC4 EN | pin 2 | 35 V | 2,1 V | 2,9 V | ✅ |
| D20 | SS34, `VIN_LOCAL` → `VIN_B` | 40 V | en directa | — | ✅ |
| C319, C325 | 10 µF / 50 V X7R 1210 | 50 V | 50 % | 62 % / 77 % | ✅ |
| C320 | 100 nF / 50 V | 50 V | 50 % | 62 % / 77 % | ✅ |
| C60 | 22 µF / 50 V electrolítico | 50 V | 50 % | 62 % / 77 % | ✅ |
| **U11** | AP63301 (VIN y EN atados) | 32 V · 35 V DC · 40 V 400 ms | 25,2 V | ≤ 31 V / 38,4 V (< 40 V por 400 ms) | ✅ |
| D23 | SS34, `+5V_AUX` → `VIN_B` | 40 V | **19–21 V en inversa** (antes 14,5) | — | ✅ ver fuga abajo |

**C273 y C314 se quedan en 35 V.** Tres razones:

1. En régimen trabajan al 73 % de su tensión, dentro del 80 % habitual para electrolíticos.
   Su vida depende sobre todo de la temperatura.
2. El enchufe en caliente con ellos puestos no pasa de ~27–30 V, porque **son ellos los que
   lo amortiguan**.
3. La tensión de *surge* de un electrolítico de 35 V suele ser de 40 a 44 V (1,15–1,25×,
   según la serie). ⚠️ No encontré la hoja de la serie RVE de KNSCHA para confirmarlo.

Lo que **no** cumple es la regla de la ronda 2 (`a5_matriz.py`), que pide 25 % de margen
contra 38,9 V. Con el SMBJ20A tampoco la cumplía: el margen era del 7 %.

Un 100 µF de 50 V es un tarro de 8 × 10 mm o más: cambia la huella. No vale la pena.

**La fuga de D23** (el riesgo N1 de la revisión de alimentación, recalculado con
`a2_drenaje.py`):

- Con 19–21 V en inversa, la fuga máxima especificada a 85 °C pasa de 0,47 a 0,53–0,57 mA.
- R25 (1 kΩ a GND, ya dibujada) la absorbe sin que `+5V_AUX` se mueva.
- Recién con D23 a **125 °C** de juntura, la fuga típica (4,8–5,3 mA) llevaría
  `+5V_AUX` a 4,8–5,3 V, todavía debajo de los 6 V del AP2125.
- ✅ Sin cambios.

**Lo que mejora con 24 V:**

- **Corriente de entrada:** baja ~17 %.
- **Corriente RMS de los cerámicos del buck A:** pasa de 1,53 a 1,43 A (D = 0,212).
- **Pérdida en los cables de la cadena:** baja ~31 %, porque va con I².
- **Margen para la caída de tensión de la cadena:** hay 9,7 V entre los 24 V de la fuente y
  el arranque de los amplificadores (14,3 V).

**Lo que empeora poco:**

- **Rizado de L1:** 1,24 → 1,31 A. El pico a 3,5 A es 4,2 A, contra 9,6 A de saturación.
- **Rizado de L2:** 1,16 → 1,20 A.
- **Tiempo mínimo de encendido:**
  - buck A, peor caso (25,5 V, 530 kHz con el spread spectrum): **377 ns contra 120 ns**;
  - buck B (550 kHz): **236 ns contra 80 ns**.
- **Disipación del buck A:** sube algunas centésimas a décimas de W por pérdidas de
  conmutación ⚠️ estimado, sin la curva de rendimiento a 24 V. La prueba de 30 min del §9
  de ALIMENTACIÓN lo cierra.

Las hojas de los dos integrados nombran como aplicación los *«5V, 12V, and 24V Distributed
Power Bus Supplies»* y traen curvas a VIN = 24 V: el integrado está pensado para esto.

**Q1, opcional:** el **Si2319CDS** de Vishay:

- es −40 V, VGS ±20 V, 4,4 A, SOT-23
  ([Farnell](https://export.farnell.com/vishay/si2319cds-t1-ge3/mosfet-p-ch-40v-4-4a-diode-sot23/dp/1858942));
- lleva la polaridad invertida de 4,5 V a 14,5 V de margen;
- ⚠️ **verificar en su hoja que el pinout sea 1 = G, 2 = S, 3 = D, como el AO3401A**, antes
  de comprarlo;
- si no coincide, quedarse con el AO3401A, que **cumple**.

---

## 5. Modos de falla con 24 V

| evento | qué pasa | quién protege |
|---|---|---|
| **24 V con el SMBJ20A todavía puesto** | lotería por placa: TVS en corto y F1 abierto en algunas | **nadie**: hay que cambiar D9 antes (§1) |
| Fuente ajustada a 28,8 V (potenciómetro al máximo) | algunos TVS conducen, fallan en corto, F1 abre. Bucks a salvo | D9 + F1; **ajustar y trabar el potenciómetro** |
| La fuente se rompe y sube hasta su protección de sobretensión | idem: TVS en corto, F1 abierto, bucks a salvo. Se arregla soldando D9 y F1 | D9 + F1 |
| Enchufe en caliente | pico ≤ 30 V con los electrolíticos sanos; F1 al 26 % de su I²t | C273/C314 + D9 + F1 de 3 A |
| Enchufe en caliente, electrolíticos secos (años) | pico ≤ 30 V; el TVS empieza a trabajar | D9 |
| Pico de corriente del enchufe por Q1 | 47–136 A durante µs, fuera de la hoja (27 A) | **heredado**; se evita enchufando con la fuente apagada (§3.4) |
| Polaridad invertida (borneras de tornillo) | Q1 apagado, nada conduce, F1 entero | Q1 (−30 V contra −25,5 V) |
| Corto en `VIN_LOCAL`, fuente grande (cadena) | F1 de 3 A abre en ms; el resto de la cadena sigue | F1 |
| Corto en `VIN_LOCAL`, fuente chica (≤ 3 A) | F1 puede no abrir; la fuente entra en *hiccup* | la fuente: **comprarla con sobrecorriente *hiccup*** |
| **Una DS11 en una cadena de 24 V** | **su MP2315S tiene 24 V de máximo de operación y 26 V absoluto**, y su SMBJ24A recorta en 38,9 V: no la protege | **nadie: las DS11 se quedan con sus fuentes de 19-20 V** |
| La cadena cae (fuente sobrecargada) | amplificadores OFF debajo de 11,1 V, micro vivo | UVLO (sin cambios) |

---

## 6. Reglas de operación (van a la guía cuando lleguen las placas)

1. **D9 = SMBJ24A antes de la primera conexión a 24 V.** Si hay placas viejas armadas con
   SMBJ20A, no van a 24 V.
2. **La fuente tiene que cumplir cuatro cosas:**
   - regulada (conmutada, no un transformador con rectificador);
   - ajustada a **24,0 V medidos en vacío**;
   - potenciómetro **trabado**;
   - sobrecorriente en modo *hiccup*.
3. **Nunca más de 25,5 V** en la bornera.
4. **Las DS11 nunca en 24 V.** Fuentes de 24 V y de 19-20 V **marcadas de forma
   distinta**, con colores o cintas.
5. **Conectar la ficha con la fuente apagada**, y recién después encenderla. Si hay que
   enchufar con la fuente encendida, se puede: el diseño lo aguanta, pero gasta el fusible
   y estresa Q1.

---

## 7. Banco: lo que cambia del §9 de ALIMENTACIÓN

- [ ] Fuente ajustada a 24,0 V en vacío. Después, **25,5 V**: la placa funciona igual y D9
      no calienta (dedo o termómetro, tras 10 min).
- [ ] Hot-plug 20 veces con la fuente real de 24 V, punta en `VIN_LOCAL` (TP7): **el pico
      no supera 30 V**. El modelo dice ~24 V con los electrolíticos nuevos: si da mucho más,
      el modelo está mal y hay que entender por qué antes de seguir.
- [ ] **Corriente de enchufe** con una pinza de corriente, o con la caída sobre F1: el pico
      tiene que estar en el orden de los 50–130 A del §3.4. Después de las 20 enchufadas,
      Q1 sin cambio de RDS(on): caída entre TP5 y TP7 con 1 A.
- [ ] Polaridad invertida a 24 V con fuente de laboratorio limitada: consumo 0, F1 entero.
- [ ] Temperatura del buck A a 24 V con el *channel walker*: **< 85 °C** de encapsulado
      (misma prueba, ahora a 24 V).

---

## 8. Qué hay que tocar, y quién

| dónde | qué | quién |
|---|---|---|
| esquemático | D9 → SMBJ24A `C908801`; F1 → JFC1206-1300FS `C136347`; net `+20V` → `+24V`; valor de TP5 | Fernando |
| PCB | sólo actualizar desde el esquemático; serigrafía «24 V»; rellenar zonas y `tools/check_pcb.sh` | Fernando |
| compras | SMBJ24A y fusible de 3 A (no estaban en la compra de la DS12) | Fernando |
| `tools/check_layout.py` | acepta `+20V` y `+24V` como riel de entrada | hecho |
| documentos | aviso en ALIMENTACIÓN, guía de operación, README y CHECKLIST (D9, F1, net, serigrafía) | hecho |

---

## 9. Lo que no está verificado ⚠️

- La curva de capacidad contra tensión de los 10 µF 1210 a 24 V: se barrió de 9 a 18 µF
  totales.
- El coeficiente de temperatura del VBR del SMBJ24A: el margen de 25,5 V supone ~0,1 %/°C.
- La tensión de *surge* de la serie RVE (C273/C314).
- La resistencia en frío de F1: el modelo supone ~50 mΩ dentro de los 0,10 Ω mínimos.
- El comportamiento del diodo de Q1 con pulsos de 50–130 A.
- El pinout del Si2319CDS.
- Los datos de la fuente Mean Well salen de revendedores, no de la hoja oficial.
- La simulación es de parámetros concentrados:
  - fuente rígida;
  - sin ESL de los capacitores;
  - TVS lineal a tramos;
  - bucks apagados.

  Su autoprueba comprueba tres resultados analíticos (LC ×2, I²t de un RC, sobreamortiguado
  sin pico) y un caso sin nada que medir (todo da cero).
