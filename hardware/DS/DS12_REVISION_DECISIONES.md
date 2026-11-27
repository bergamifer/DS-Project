# DS12 — revisión de las decisiones recientes (2026-11-10)

> Una segunda mirada sobre lo decidido entre el 23-oct y el 7-nov: entrada a 24 V, LEDs de J9,
> R20, polarización del ADC y micrófono de banco. Para cada cosa: por qué se decidió, con qué
> evidencia, y si se sostiene.
>
> **Resultado:** las decisiones de fondo se sostienen. Aparecieron **dos errores para corregir antes
> de fabricar** (§1 y §2), **una mejora barata** (§3) y **una medida que falta** (§4).
>
> Cómo se verificó:
> - **Esquemático:** netlist regenerada del esquemático guardado el 28-oct con `tools/sch2net.py`, y
>   comparada con la del 8-oct con `tools/netlist_diff.py`.
> - **PCB:** `kicad-cli pcb drc --refill-zones` sobre una **copia** de `DS.kicad_pcb`, y
>   `tools/pcb_status.py` contra la netlist nueva.
> - **Hojas de datos:** AK5538, PAM8302A, STM32H743, el plano del jack (`datasheets/rj45x4.pdf`) y la
>   hoja del micrófono.
> - **Firmware:** búsqueda en `main.c`, `stm32h7xx_hal_msp.c` y `Core/Inc/board/`.
>
> Lo que **no** está verificado va marcado ⚠️ y se junta en el §9.

---

## 0. En una tabla

| # | Qué | Gravedad | Qué hacer |
|---|---|---|---|
| 1 | Pads de LED de jacks vecinos en J9, a 0,097 mm y ahora con redes distintas | 🔴 antes de fabricar | usar un solo LED por jack (§1) |
| 2 | El documento del micrófono proponía modelos de puerto inferior y una huella equivocada | 🔴 antes de dibujarlo | ya corregido en [DS12_MICROFONO_DNP.md](DS12_MICROFONO_DNP.md) |
| 3 | R20 en 33 Ω deja a PD3 protegido sólo por firmware | 🟡 mejora | R20 a 470 Ω (§3) |
| 4 | El brillo de los LEDs nunca se probó con la corriente que va a tener la DS12 | 🟡 medir | prueba M13 (§4) |

---

## 1. J9: los pads de LED de jacks vecinos quedaron con redes distintas

### El mecanismo

J9 son cuatro jacks pegados. Cada jack tiene dos LEDs, uno en cada esquina, y cada LED dos patas.
La última pata de un jack y la primera del siguiente quedan a **1,397 mm** entre centros:

| par de pads | qué son |
|---|---|
| 12 y 21 | cátodo del LED 2 del jack 1 · ánodo del LED 1 del jack 2 |
| 24 y 33 | cátodo del LED 2 del jack 2 · ánodo del LED 1 del jack 3 |
| 36 y 45 | cátodo del LED 2 del jack 3 · ánodo del LED 1 del jack 4 |

Con pads de 1,3 mm y agujeros de 1,0 mm (lo que pide el plano del fabricante: «16-Ø1.0»), entre
cobre y cobre quedan **0,097 mm**, y entre agujero y agujero 0,397 mm.

En la DS11 esto no importaba: los dos pads de cada par eran GND. Al invertir los LEDs, el pad 12
sigue siendo GND, pero el 21 pasó a ser el ánodo del LED del jack de al lado.

### La consecuencia

- No entra máscara entre los dos pads: al soldar el jack, **un puente de estaño es probable**.
- Cada puente pone un ánodo a masa. Ese LED no prende, y PE12 entrega 3,3 V / 470 Ω = 7 mA de más.
- Con los tres pares en puente son 21 mA, más el LED que sí anda: **≈24 mA, contra 20 mA máximos
  del pin** (hoja del STM32H743, corriente por pin).
- Fabricación: JLCPCB pide **0,5 mm entre agujeros de redes distintas** ⚠️ (dato de copias de su
  tabla de capacidades; no pude abrir la página oficial). Acá hay 0,397 mm.

Evidencia: el DRC sobre la copia da 3 errores de separación y 6 de puente de máscara, todos en esos
tres pares.

> 🔧 **2026-11-13 — Fernando cambió la huella: pads ovalados.** Los pads 12, 21, 24, 33, 36 y 45 (y el
> resto de los pads de LED) pasaron a óvalo de **1,15 × 1,8 mm**, con el agujero de Ø1,0 igual (leído del
> PCB guardado a las 10:49; el `.kicad_mod` también cambió). Ya no hay violaciones entre pads vecinos. Es
> la alternativa «mantener los dos colores» con otra forma: el cobre entre vecinos queda en ≈0,25 mm y la
> distancia entre agujeros sigue en 0,397 mm.
>
> **Efecto secundario (DRC sobre una copia, rellenos recalculados): 53 → 63 violaciones, 5 son cortos.**
> Como el óvalo es más alto, lo tocan pistas viejas que pasaban cerca: PE13 (B.Cu) contra los pads 46 y 23,
> y las redes de los pads 21, 9 y 33 (B.Cu) contra los pads 22, 10 y 34 (GND). La red del pad 45 (F.Cu) queda
> a 0,08–0,13 mm del pad 36 (mínimo 0,18). Además, PE13 (B.Cu) pasa pegada a los pads 23–24, 33–36 y 45–46
> (11 puentes de máscara). Es de ruteo, no de la huella. **Pendiente de Fernando**, y depende de si se
> conserva el LED 2. Sigue sin medirse M13b (sentido del LED 2) y sigue abierta la regla de agujero a
> agujero de JLCPCB (0,5 mm según copias secundarias).

> 🔧 **2026-11-18 — LED 2: la inclinación de Fernando es conservarlo («creo»; no es un cierre).** Con eso
> como supuesto, `intento_j9/intento_j9.py` reruteó la fila sobre **una copia** del PCB del commit `b31521d`
> (DRC con rellenos recalculados y las reglas propias del proyecto):
>
> | | antes | después |
> |---|---|---|
> | cortos (`shorting_items`) | 5 | **0** |
> | puentes de máscara | 11 | **0** |
> | separaciones en J9 | 3 | **0** |
> | conexiones sin rutear | 1 (R166.2 → pad 23) | **0** |
> | total del DRC | 48 | 29 (27 del QR, 2 vías `+5VA` sueltas: no son de J9) |
>
> Cómo: el bus de PE13 pasa por arriba de la fila, recto; los tres desvíos del LED 1 pasan por encima del
> GND vecino en otro carril; R114 va directo en B.Cu; y **R164, R165 y R167 se giran 180°** para que su pata de
> PE13 quede del lado del bus. Es un reruteo, no una huella nueva. `check_layout.py` da lo mismo antes y después
> (no toca los bucks). La copia está en `.ds12_audit/pcb_intento/DS_j9.kicad_pcb` (gitignored); el script se
> puede correr sobre cualquier versión de tu PCB. **Sin verificar:** que JLCPCB acepte los 0,397 mm entre agujeros
> de pads vecinos (su mínimo sería 0,5 mm ⚠️, dato sin confirmar en la página oficial: el DRC de KiCad no lo mira),
> el sentido del LED 2 (M13b) y el brillo (M13). **Conservar el LED 2 depende de esos tres.**

### Qué hacer

**Recomendado — un solo LED por jack, el LED 1.** Es el que probaste en el banco (prende con 5 V y
330 Ω), así que su sentido está confirmado.

- El LED 1 queda como está dibujado: R111/R113/R112/R114 a los pads 9/21/33/45, y los pads
  10/22/34/46 a GND.
- El LED 2 se anula uniendo sus dos pads a la red del pad vecino:
  - pads 11 y 12 → la red del pad 21;
  - pads 23 y 24 → la red del pad 33;
  - pads 35 y 36 → la red del pad 45;
  - pads 47 y 48 → GND (no tienen vecino).
- Salen R164–R167, y PE13 queda libre.

Con los dos pads de un LED en la misma red no circula corriente por él. Y cada par apretado queda
en una sola red, así que un puente de estaño no cambia nada y el DRC queda en cero.

**Opcional sobre lo anterior — un pin por jack.** Llevando cada resistencia a su propio pin (PE12,
PE13, PE14 y PE10, todos del mismo lado del chip) se puede prender un jack solo, que era la idea
original de la auditoría (§3.6). Son tres pistas más.

**Alternativas, si querés otra cosa:**

- *Usar el LED 2 en vez del 1* (lo que ya está dibujado para PE13, con los dos pads del LED 1 a
  GND). Es más natural de dibujar, pero el sentido del LED 2 sale sólo del plano del jack ⚠️: nunca
  se probó, porque en la DS11 sus dos pads están a GND. Probarlo antes en un jack suelto (M13b).
- *Mantener los dos colores.* Hay que achicar la huella a agujero de 0,9 mm y pad de 1,2 mm: queda
  0,197 mm entre cobres y 0,497 mm entre agujeros, justo en el límite de las dos reglas. Antes hay
  que medir la pata del LED con calibre (M14), porque el fabricante pide agujero de 1,0 mm.

---

## 2. El micrófono: el documento estaba mal

Lo escribí el 7-nov con tres modelos «equivalentes» y pidiendo puerto superior. Dos de los tres
(ICS-43434 y SPH0645LM4H-B) son de **puerto inferior**: escuchan por un agujero del lado de los
pads, así que necesitan un agujero en la placa. Además figuran como discontinuados.

El único de puerto superior es el **MSM261S4030H0R**, que mide 4 × 3 mm y no 3,5 × 2,65. Y su hoja
pide tres cosas que el documento no decía:

- la pata 4 (CHIPEN) a `+3V3`: en bajo, el micrófono queda apagado;
- 100 kΩ de la salida de datos a GND, porque la suelta durante media trama;
- 10 µF además del 100 nF.

[DS12_MICROFONO_DNP.md](DS12_MICROFONO_DNP.md) ya está corregido, con la tabla de las ocho patas.

**La decisión en sí se sostiene:** I2S sobre las redes I2S2 que ya existen sigue siendo lo más
simple. Confirmado hoy: las tres redes siguen en el esquemático (PB12, PB13, PB14 y J6.1, J6.2,
J5.5), y **el firmware no usa I2S2**.

---

## 3. R20: conviene 470 Ω, no 33 Ω

### El riesgo que sigue abierto

Con la placa sólo por USB, `+5VA` está en 0. Si en ese estado SD sube, PD3 alimenta a los 32
amplificadores a través de sus diodos internos (M6: 0,554 V, el diodo existe). Con R20 = 33 Ω la
cuenta da (3,3 − 0,55) / 33 ≈ 80 mA: decenas de mA sobre un pin de 20 mA.

Hoy lo único que lo impide es la guarda de firmware (SD sólo con PC1 > 2,25 V). Funciona, pero no
cubre un firmware con un error ni un programa frenado con el depurador.

### Lo que no se había mirado

El 1-oct se analizó una sola alternativa, 5,1 kΩ, y M5 la descartó bien: con los ≈7,6 kΩ que
suman los 32 pines hacia masa, SD quedaba en ≈2 V. Nadie miró los valores del medio.

| R20 | SD en alto (con 7,6 kΩ a masa) | corriente de PD3 con `+5VA` en 0 |
|---|---|---|
| 33 Ω (hoy) | 3,29 V | ≈80 mA ⛔ |
| 200 Ω (`C8218`, 0603) | 3,22 V | 13,8 mA |
| **470 Ω (`C25117`, 0402)** | **3,11 V** | **5,9 mA** ✅ |
| 1 kΩ (`C11702`, 0402) | 2,92 V | 2,8 mA |
| 5,1 kΩ (descartada) | 1,97 V | 0,5 mA |

El PAM8302A pide **1,2 V mínimo** para leer SD en alto (su hoja, «SHD Input High»). Con 470 Ω
sobran 1,9 V, y la corriente de falla queda debajo de los 8 mA con los que el STM32 garantiza sus
niveles.

### Qué hacer

- **R20 a 470 Ω, `C25117`.** Es la misma pieza de los LEDs y de las entradas del ADC (hay 24 en la
  placa), pero es 0402 y R20 hoy es 0603: **hay que cambiarle la huella**.
- Si no querés tocar la huella: 200 Ω `C8218`, que ya está en la placa en 0603. Protege menos
  (13,8 mA), pero queda dentro del máximo del pin.
- **La guarda de firmware se queda igual.** Sigue haciendo falta para el orden de encendido y para
  evitar el golpe al apagar. Con la resistencia deja de ser la única barrera.
- R19 (1 kΩ de PD3 a GND) no cambia: con el STM32 en reset, SD sigue en bajo.

⚠️ **No está probado.** Se puede validar en una DS11 cambiando R20, pero para ver que los
amplificadores encienden hay que encenderlos: lo decide Fernando. También es una inferencia que los
7,6 kΩ de M5 sean pull-downs internos; la cuenta de arriba es el caso desfavorable.

---

## 4. El brillo de los LEDs no está probado

La prueba que confirmó el sentido fue con **5 V y 330 Ω**: unos 9 mA en total. De esos, al LED le
llegaron ≈6 mA; el resto se fue por R111 al diodo de protección de PE12 (el mismo que dio 0,7 V en
M12). En la DS12 cada LED recibe 3,3 V por 470 Ω: (3,3 − 2,0) / 470 ≈ **2,8 mA**, la mitad. Y eso
suponiendo una caída de 2,0 V, que nunca se midió. El plano del jack dice que un LED es amarillo y
el otro verde; si el verde fuera de los de 3 V, con 3,3 V casi no prende.

La prueba es **M13** en [`revision_alimentacion/medidas_ds11.md`](revision_alimentacion/medidas_ds11.md):
la misma de antes con 560 Ω en lugar de 330 Ω (al LED le llegan ≈2,5 mA) y el multímetro sobre el
LED. Sin alimentar la placa, sin amplificadores.

---

## 5. Las decisiones, una por una

| Decisión | Por qué (mecanismo) | Evidencia | Veredicto |
|---|---|---|---|
| **Entrada a 24 V**, mismos bucks | Los dos integrados operan hasta 32 V y aguantan 35 V. El vatio de 24 V es más barato | hojas del AP63356 y del AP63301; simulación `s13_entrada_24v.py` | ✅ se sostiene |
| **D9 a SMBJ24A** | El de 20 V empieza a conducir entre 22,2 y 24,5 V según la unidad: a 24 V, algunas placas lo quemarían al enchufar | hoja de MDD | ✅ obligatorio; ya está en esquemático y PCB |
| **F1 a 3 A** | El pulso de carga de los capacitores crece con el cuadrado de la tensión: gastaba el 87 % del fusible de 2 A, y el 26 % del de 3 A | simulación | ✅ ya está |
| **Máximo 25,5 V** | El TVS de 24 V empieza a conducir en 26,7 V, menos con frío | hoja + margen ⚠️ | ✅ regla de operación |
| **LEDs de J9 invertidos** | En la DS11 el ánodo estaba a GND: nunca podían prender | prueba de banco, 5 V + 330 Ω | ✅, con §1 y §4 |
| **Bias del ADC, 10 kΩ por pata** a un nodo de 1,65 V | Sin red, las entradas caen a 0 V (M4) y la hoja pide 1,58–1,72 V. M4b mostró que la entrada no carga en continua, así que una red pasiva alcanza | M4 = 0,03 V; M4b = 3,296 V; hoja del AK5538 | ✅ se sostiene |
| **R20 en 33 Ω + guarda de firmware** | 5,1 kΩ dejaba SD en ≈2 V | M5, M6 | 🟡 mejorable: §3 |
| **Micrófono I2S, DNP** | Cero patas nuevas y sin circuito analógico | netlist; firmware | ✅ la idea; el modelo estaba mal: §2 |
| **Ronda 2 de alimentación** (cinco 22 µF en `+5V_AUX`, drenajes, VBUS, C28, TDMIN, PE15) | ver [DS12_REVISION_ALIMENTACION.md](DS12_REVISION_ALIMENTACION.md) §9 | netlist de hoy | ✅ todo sigue dibujado |

### Sobre el bias del ADC, que es lo más nuevo

- **Por qué alcanza con resistencias.** M4 y M4b juntas descartan que la entrada tenga un camino de
  continua a algún lado: suelta va a 0 V (la arrastra el propio multímetro), y con 10 kΩ a 3,3 V se
  queda en 3,296 V. Los ≈10,4 kΩ de la hoja son entre la pata P y la N.
- **Qué cuesta.** El pasa-altos de entrada sube de ≈13 a ≈18 Hz y la señal pierde 0,4 dB más
  (de −0,75 a −1,12 dB). Para la obra no pesa.
- **Por qué 10 kΩ y no más.** Con 100 kΩ se pierde menos señal, pero 1 µA de fuga ya corre la
  tensión 0,1 V y la saca de la ventana de ±0,066 V. Con 10 kΩ hace falta 6,6 µA.
- **Un solo nodo para las 16 patas.** La diafonía por el nodo compartido la estimo en −70 dB o
  mejor a 100 Hz, y menos con fuentes balanceadas ⚠️ (cuenta, no medida).
- **Dónde está el dibujo.** El nodo ya está (R169 a `+3.3VA`, R170 a GND, C58 de 100 nF y C62 de
  10 µF). De las 16 resistencias hay **una**: R171, en AIN4N. Faltan 15.

---

## 6. Lo que ya venía del rediseño

No encontré nada para revisar. La netlist de hoy contra la del 8-oct sólo difiere en los LEDs de
J9, las piezas nuevas del bias y los tres cambios de 24 V.

| Cambio respecto de la DS11 | Qué resuelve |
|---|---|
| Dos bucks (AP63356 para `+5VA`, AP63301 para `+3V3`) en lugar de un buck y un regulador lineal | el reset al encender amplificadores, 1,2 W de calor y el buck al límite |
| Fusible, P-FET, TVS y electrolíticos de ESR alta en la entrada | polaridad invertida, sobretensión y el enchufe en caliente |
| UVLO sólo en el buck de los amplificadores | el micro sigue vivo y avisa cuando cae la fuente |
| BAT54C entre `PDN_AK` y PG; lectura de `+5VA` en PC1 | el ADC se resetea cuando vuelve `+5VA`; PG no puede indicar falla si el buck no tiene entrada |
| USB del STM32 (PA11/PA12, choque FL1, VBUS en PA5) | flasheo sin ST-Link |
| Línea de sincronismo por PD2, no por PC13 | la errata del cristal de 32 kHz |
| Cuatro PTC en la expansión, pull-up en AMUTEO | cortos por un cable mal enchufado; una línea que dependía del firmware |

---

## 7. Estado real al 2026-11-10

**Esquemático (guardado el 28-oct):**

- hecho: D9, F1 y `+24V`; LEDs de J9 (LED 1 a PE12, LED 2 a PE13); nodo del bias y una resistencia;
- falta: 15 resistencias de bias; el micrófono; el cambio de J9 del §1; R20 del §3.

**PCB (copia, rellenos recalculados):** 0 cortos.

- le faltan C58, C62, R169, R170 y R171 (actualizar desde el esquemático);
- 1 conexión sin rutear: R166.2 a J9.23;
- 9 errores en J9 (§1);
- 27 avisos del QR, que está sobre vías ⚠️ (no probé si se lee);
- 15 avisos de la tabla de librerías y 2 vías sueltas.

**Firmware:**

- la guarda de `+5VA` está: `power_guard.c`, el watchdog analógico sobre PC1 en `main.c` y sus tests
  de host. **Sin probar en una placa**, porque todavía no hay DS12;
- ~~PE12 y PE13 siguen como pines sin usar~~: **hecho el 2026-11-18** (`jack_leds.c`, rama `claude/ds12-leds-y-analisis`): PE12 = latido, PE13 = amplificadores, con test de host y mutantes. Sin probar en placa;
- I2S2 sin usar.

**Compras:** SMBJ24A `C908801`, fusible de 3 A `C136347`, y el micrófono.

---

## 8. Pendientes del checklist B, con recomendación

Estado verificado en la netlist de hoy: ninguno está dibujado.

| Ítem | Recomendación | Por qué |
|---|---|---|
| Nombrar las 64 redes de parlante | **hacer** | son sólo etiquetas, no mueven ninguna pista, y es lo que habría mostrado el cruce de pares |
| Fila de 8 pads para el SAI | hacer si hay lugar | es el único punto para pinchar el bus que dio el error más caro |
| J7 a un conector de 8 o 9 pines | **ya es opcional** | los PTC F4–F7 limitan la corriente si se enchufa el ST-Link en J5 o J6; y obliga a tener un cable para las DS11 y otro para las DS12 |
| SD en cuatro bancos | sólo si querés encendido escalonado | pide cuatro pines, ruteo en la zona de amplificadores y firmware nuevo |
| Autoprueba por PC0 | opcional | no bloquea nada |
| C45/C46 a 0402, barrel jack, series de MCLK | como están en la lista | sin novedades |

**Sugerencia de orden:** cerrar el esquemático antes de seguir ruteando. Al PCB le falta una sola
conexión, y cada pieza nueva obliga a reabrirlo.

### Dos mejoras opcionales

- **100 kΩ a GND del lado del conector de cada capacitor de entrada** (16 piezas, `C25741`). Con el
  bias, el lado del chip queda firme en 1,65 V, pero el lado del conector flota mientras no hay
  nada enchufado. Al enchufar un micrófono el capacitor se acomoda de golpe, y ese golpe entra al
  ADC. En los modos que llevan la entrada a los parlantes, sale por los 32. La resistencia deja el
  capacitor ya cargado. ⚠️ Es una cuenta, no una medida; enchufar con los amplificadores apagados lo
  evita igual.
- **Que una DS11 no pueda terminar en una cadena de 24 V.** Las DS11 y las DS12 usan la misma
  bornera. Como mínimo, fichas de color distinto; mejor, piezas de codificación en las Phoenix
  ⚠️ (existen para la serie MSTB; no busqué el código).

---

## 9. Lo que no está verificado ⚠️

- El sentido del LED 2 de J9 y la caída de tensión de los dos LEDs.
- El grosor de la pata de LED del jack (sólo importa para mantener los dos colores).
- Las reglas de JLCPCB para agujeros de redes distintas: salen de copias de su tabla.
- Que los 7,6 kΩ de M5 sean pull-downs internos de los PAM8302A.
- R20 = 470 Ω en una placa real.
- La diafonía por el nodo de bias y el golpe al enchufar un micrófono: son cuentas.
- El stock del MSM261S4030H0R.
- Que el QR se lea estando sobre vías.
- AIN8N en M4b: no se pudo medir sin tocar la pata vecina.
