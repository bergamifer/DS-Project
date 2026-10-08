# DS12 — Revisión de alimentación: resultados

> **2026-10-01 · primera tanda: S1, S2 y S4** del §6 de
> [DS12_REVISION_ALIMENTACION_PLAN.md](DS12_REVISION_ALIMENTACION_PLAN.md). Sólo esquemático.
>
> **Contra qué se verificó:**
> - **Netlist:** `DS.net` exportado el 2026-10-01 a las 15:29:40 (digest regenerado en `.ds12_audit/`).
> - **Hojas de datos:** las citadas, ahora en `datasheets/`.
> - **Norma USB 2.0:** texto completo, de usb.org (`usb_20_20250603.zip`).
> - **Cuentas y simulaciones:** las de [`revision_alimentacion/`](revision_alimentacion/). Todas tienen autoprueba con resultado conocido, y dos de esas autopruebas encontraron errores míos antes de usarlas.
>
> **Refutación:** tres agentes nuevos, uno por sospecha, que recibieron sólo la afirmación y la
> evidencia, con la orden de tirarla abajo.
>
> **Control de calidad:** en la evidencia de S4 se plantó un dato falso ("C60 es cerámico X7R"). El
> refutador lo detectó, así que la refutación mide algo.

---

## En cuatro líneas

| | veredicto | severidad |
|---|---|---|
| **S1** — D2/D8/D11 conducen con el 20 V presente | **CONFIRMADO.** La PC puede entregar parte o toda la carga de `+5V_AUX`. No hay riesgo de daño | 🟢 documento |
| **S2** — SD de los PAM en alto con `+5VA` en 0 | **CONFIRMADO** en modo sólo-USB, con el firmware de hoy. Es heredado de la DS11 | 🟡 firmware **antes** del primer arranque por USB |
| **S4** — más de 10 µF colgados del USB | **CONFIRMADO** que no cumple la norma, pero **sin consecuencia**. **REFUTADO** que "la placa no arranca desde USB" | 🟢 |
| **S4.f** 🆕 — la refutación encontró uno peor | Con el 20 V puesto, enchufar en **J8** un cable con VBUS vivo lleva `+5V_AUX` a **6,6–7,1 V**. Pasa el máximo del AP2125 | 🟡 un capacitor |

Además aparecieron seis cosas nuevas que **no** están verificadas a fondo (§5). La primera, la
fuga de D23, merece la próxima ronda.

---

## 1. S1 — `+5V_AUX` se alimenta de a dos con el 20 V presente

### El mecanismo

`+5V_AUX` es un punto al que llegan cuatro fuentes, cada una por su diodo: `+5VA` (por FB69 y
D19), los dos USB (por F2→D2 y F3→D8) y el ST-Link (por D11).

Un diodo no es una canilla de umbral fijo: deja pasar más o menos corriente según cuánto "empuja"
cada lado. Como los cinco diodos son la **misma pieza** (SS34, `C8678`, verificado en
`componentes.tsv`), las caídas de los diodos se cancelan. Lo que decide quién alimenta es
simplemente **quién está más alto: VBUS o `+5VA`**.

`+5VA` vale 5,088 V, y la norma USB permite cualquier VBUS entre 4,75 y 5,25 V. Por eso, con una
PC enchufada, el reparto puede caer en cualquier lado:

| VBUS de la PC | 4,90 V | 5,00 V | 5,10 V | 5,25 V |
|---|---|---|---|---|
| sin expansión (45 mA) | 0 mA | 3 mA | 26 mA | 45 mA (todo) |
| expansión cargada (1 A) | 222 mA | 371 mA | 526 mA | 755 mA |

`+5VA` nominal; con la tolerancia, `+5VA` va de 4,95 a 5,23 V (`s1_reparto.py`). El
resultado casi no depende del modelo del diodo: con n = 1,0, 1,1 o 1,5 da lo mismo, y el
refutador barrió n hasta 2,0, temperatura, la resistencia de FB69 y la del cable.

### Evidencia

- **Topología:** `+5V_AUX` tiene 12 nodos (`redes.txt`) y ninguna otra fuente.
- **Diodo:** curva de MDD SS34, Rev 2024A5, p.2, Fig.3: ≈0,34 V a 0,1 A y 0,44–0,45 V a 1 A. El refutador la leyó a 400 dpi.
- **`+5VA`:** VFB = 0,792 / 0,800 / 0,808 V (DS41949 Rev 3-2, p.5).
- **Carga sin expansión:** AK5538 "AVDD + VREFHm 38 mA típ / 50 mA máx" (015099878-E-02, p.9).
- **D2 deja de conducir sólo si** VBUS < `+5V_AUX` ≈ 4,63–4,90 V. Eso es un puerto USB débil.

### Lo que cambió con la refutación

- **El límite de USB que yo había citado estaba mal.** Con el 20 V puesto la placa es
  *auto-alimentada*, y USB 2.0 §7.2.1 le permite tomar del VBUS **una sola carga unitaria (100 mA),
  no 500 mA**. Con una sola expansión cargada ya se pasa.
- **La DS11 no prueba esto.** Tenía otros diodos (B5819W) y otras cargas: sirve de
  ilustración, no de evidencia.

### Qué hacer

- **En el hardware, nada:** D2, D8 y D11 siguen impidiendo que la placa le mande corriente a la
  PC, que es lo que importa. Revisé ese sentido de cada red de VBUS (ver además N3 en el §5).
- **Corregir los documentos** (§6): la tabla de modos dice "D2 en reverso, la PC no entrega nada",
  y `DS12_USB_STM32.md` dice "la PC entrega poco o nada".
- **Nota operativa:** con la expansión cargada y una laptop enchufada, la laptop puede deshabilitar
  el puerto por exceso de consumo. La placa no se entera, porque D19 sigue alimentando.

**Medición que lo cierra:** un medidor USB-C en línea entre la Mac y J3, con el 20 V puesto. Tiene
que dar algo entre 0 y 45 mA, según el VBUS de la Mac.

---

## 2. S2 — los 32 pines SD en alto con los amplificadores sin alimentación

### El mecanismo

SD es la "llave de encendido" de cada PAM8302A, y el STM32 la maneja a 3,3 V. Cuando la placa
está sólo por USB, el buck de los amplificadores no tiene entrada y `+5VA` está en 0. Si en ese
estado el firmware sube SD, cada amplificador recibe una orden a 3,3 V con su propia alimentación
en cero.

Los chips no toleran tener una entrada por encima de su alimentación. Los diodos de protección
internos conducen desde la entrada hacia la alimentación, y la señal intenta **alimentar el chip
desde un GPIO**.

### Lo que está demostrado

- **El máximo absoluto se viola.** DS41333 Rev 6-2, p.2: *"Input Voltage −0.3 to VDD +0.3"*. Con
  VDD = 0 eso es 0,3 V, y SD está en 3,3 V. Además, p.8: SD debería subir *"1ms ~100 ms"* **después**
  de VDD.
- **El firmware de hoy lo puede hacer por cuatro caminos, y ninguno mira `+5VA`:**
  - el **autoarranque** guardado en la EEPROM: se arma al bootear sin condiciones y enciende a los 15 s (`main.c` ≈2709-2725 y ≈6080-6105);
  - `POST /amps`;
  - el pulsador **K2** largo;
  - `POST /autostart`, que arma la cuenta en el momento, sin reiniciar. Este lo encontró el refutador.

  El autoarranque es justamente lo que se activa para la instalación. Escenario realista: una placa
  que vuelve de la muestra y se enchufa a la Mac sólo por USB para reflashearla. A los 15 s suben
  los 32 SD.
- **Precondición verificada:** en sólo-USB el `+3V3` existe, así que el firmware corre. El AP63301
  arranca con VIN ≤ 3,7 V (DS42002 Rev 3-2, p.4) y tiene modo de baja caída, con un ciclo de
  trabajo de ≈100 % (p.18). `VIN_B` queda en ≈3,9 V.
- **Es heredado.** La DS11 tenía exactamente la misma condición en sólo-USB (`dig11`), y se usó así
  (`docs/loopback_debug.md` §8 y §16.0). Eso contradice el §6.1.a de `DS12_ALIMENTACION.md`, donde
  dice "en la DS11 no pasaba": es cierto para el UVLO, no para el USB.

### Lo que NO es un problema

- **La ventana de arranque**, antes de que corra el firmware. **R19 (1 kΩ de PD3 a GND)** mantiene
  SD en bajo. Después del reset los GPIO están en modo analógico (DS12110, p.32), y la app baja PD3
  antes de configurarlo como salida (`main.c:4510` y `:4534`).

### Lo que queda abierto: cuánta corriente circula de verdad

DS41333 no muestra la estructura de entrada de SD ni especifica corriente de inyección. **Si**
existe el diodo de SD a VDD, pasarían tres cosas:

- **PD3 se sale de su límite.** Tendría que entregar **≥27 mA**, contra los ±20 mA del STM32
  (DS12110 §6.3.15, Tabla 61), y por un buen rato: los cuatro PCM1690 cuelgan de `+5VA` por FB3 y
  mantendrían el riel abajo.
- **IC4 queda fuera de su máximo absoluto.** Por L1 y con VIN en 0, vería VSW > VIN + 0,3 V (DS41949, p.5).
- **Q1 podría conducir al revés** y empujar corriente hacia la cadena de 20 V. El refutador la estimó despreciable.

### Qué hacer

1. **Obligatorio, en firmware, antes del primer arranque de una DS12 por USB con el autoarranque
   activo:**
   - **No subir SD si PC1 marca menos de 2,25 V.** Aplica a los cuatro caminos.
   - **Que el autoarranque *espere* en AS_WAIT** hasta que aparezca `+5VA`. Si sólo lo rechaza
     adentro de `Set_Amps`, el intento ya quedó contado (el contador se guarda *antes*, `main.c` ≈6092-6101). Al minuto queda en AS_DONE con los amplificadores apagados, o con unos reinicios en AS_GAVEUP.
   - **Un corte por flanco** con el *analog watchdog*, para cuando se retira el 20 V con el USB puesto.
   - **Un interruptor de placa**, como `SPK_LAYOUT_2X16`. El mismo binario corre en las DS11, donde PC1
     está al aire: sin el interruptor quedarían mudas.
2. **Opcional, en el hardware: R20 de 33 Ω a 5,1 kΩ** (`C23186`, 0603, ya está en el BOM), sin
   tocar el footprint. Limita la inyección a ≈0,55 mA en total. **Queda pendiente de una
   medición:** sólo es seguro si SD toma menos de ≈13 µA por pin, y DS41333 no lo especifica.

### La medición que cierra lo abierto

Se hace sobre una DS11 **sin alimentar**, que tiene el mismo PAM y la misma R19/R20:

- **(i) Óhmetro entre SD y GND.** Tiene que dar **1,033 kΩ** (R20 + R19) si los PAM no tienen
  pull-down interno. Si da menos, lo tienen. Esto decide la opción 2.
- **(ii) Tester en modo diodo, rojo en SD y negro en `+5VA`.** 🔧 **Tal como lo había escrito yo,
  da un falso positivo**, y lo encontró el refutador. Hay un camino indirecto: R20 → R19 → GND → los
  diodos de sustrato → `+5VA`. Con 1,24 mF en el medio, la lectura **arranca** en I × 1033 Ω
  (0,3–0,5 V con testers de 0,3–0,5 mA) y sube durante segundos. Para que sirva:
  - medir antes la corriente del tester sobre una resistencia de 1 kΩ;
  - esperar 10 s o más;
  - comparar contra I × 1033 Ω + 0,6 V.

  Mejor todavía: medir un PAM8302A suelto, o levantar R20.

---

## 3. S4 — el USB ve más de 10 µF

### Lo confirmado: no cumple la norma

- **Capacidad directa:** en sólo-USB, el VBUS ve **47,9 µF sin limitación de corriente**. Son C31,
  C57, C59, C319, C325, C320 y C60. Sumado sobre `redes.txt`, y el refutador llegó al mismo número.
- **Capacidad detrás de reguladores:** 47 µF detrás del AP2125, que limita a 50 mA en corto
  (DS38144), y 113,7 µF detrás del arranque suave de U11.
- **Lo que pide la norma:** USB 2.0 §7.2.4.1 y la Tabla 7-7 permiten **10 µF en paralelo con 44 Ω**,
  *"plus any capacitive effects visible through the regulator"*, o si no, limitación de corriente de
  arranque. La DS12 tiene ≈4,8 veces eso.

**Lo que esa regla protege**, según el propio texto, es que el VBUS del hub no caiga más de 330 mV
**para los otros puertos**, y la vida de los contactos. No habla del propio dispositivo.

- **Simulación** (`s4_hotplug.py`): la caída del hub da ≈1,0 V con la DS12, contra ≈0,5 V con un
  dispositivo de norma en el mismo modelo. El refutador, con 200 µs de ventana, obtuvo 1,26 V contra
  0,55 V.
- **Consecuencia práctica:** en el banco, enchufar la DS12 *sin* 20 V a un hub compartido puede
  reiniciar otro aparato del mismo hub. Para la obra no tiene consecuencia.

### Lo refutado: "la placa no arranca desde USB"

- **Cable C a C (la Mac):** la fuente Type-C enciende VBUS recién después de ver Rd durante
  tCCDebounce (Type-C §4.5.2.2.8.2). J3 y J8 tienen sus dos Rd de 5,1 kΩ. Con rampas de 0,5–3 ms el
  pico es de 0,14–0,63 A. El refutador agrega que con rampas de 20–175 µs llega a 1,4–5,9 A durante
  0,1–0,26 ms, y que una llave de puerto típica (TPS2051B) recién marca falla a los 4–15 ms.
- **Cable A a C:** el pico es de 7–15 A durante microsegundos, y tampoco llega a disparar la protección.
- **Sobretensión en sólo-USB:** no hay riesgo. Peor caso: 5,04 V en mi simulación y 5,45 V en la del
  refutador, que tiene supuestos más duros. El AP2125 admite 6,0 V en operación y 6,5 V absolutos.

### 🆕 S4.f — lo que encontró la refutación: J8 con el 20 V puesto

Yo había afirmado que con el 20 V presente enchufar el USB "no genera inrush apreciable". Es cierto
para la corriente, pero **falso para la tensión**. Lo reproduje con código propio
(`s4f_verif.py`) y coincide con el refutador.

**El mecanismo, que es un golpe de ariete:**

1. **Cuando no hay cable, C55 está vacío.** Es el capacitor del VBUS de J8, y R160 + R161 (162 kΩ a GND) lo descargan.
2. **Al tocar un cable con VBUS ya vivo, el cable se carga como una cañería.** Arranca a circular
   corriente, y la inductancia del cable la acumula.
3. **Esa corriente no se puede frenar de golpe.** Cuando C55 se llena, sigue de largo por D8 hacia `+5V_AUX`.
4. **`+5V_AUX` tiene sólo 1,1 µF** (C57 + C59). Los 42 µF de `VIN_B` no ayudan, porque D23 los
   desconecta: está en reverso con `VIN_B` en 19 V.
5. **D8 no deja volver la corriente, así que el capacitor se queda con el pico.**

| condición | pico de `+5V_AUX` |
|---|---|
| cable típico (0,7 µH, 0,15 Ω), VBUS 5,25 V | **6,59 V** |
| peor caso del barrido (con el clamp de U9) | **7,13 V** |
| sin clamp, cable corto y grueso | 8,2 V |

El AP2125 admite 6,0 V en operación y **6,5 V absolutos** (DS38144). El pico dura del orden de
decenas de µs y también perturba `+3.3VA`, que es el AVDD/VREFH del ADC.

**Cuándo pasa:**
- con la obra encendida, cada vez que alguien enchufa en el USB del STM32 un cable A a C, un
  cargador o un hub;
- **con la Mac por C a C no pasa**, porque la rampa de la fuente es lenta: ≤5,23 V con rampas de 10 µs o más;
- **en J3 tampoco**, porque la fuga de D2 mantiene C31 precargado.

**El arreglo: más capacidad cerámica en `+5V_AUX`** (`s4f_mitig.py`):

| C55 (efectivo) | `+5V_AUX` (efectivo) | peor caso | cable típico |
|---|---|---|---|
| 4,0 µF (hoy) | 0,9 µF (hoy) | 7,13 V | 6,59 V |
| 4,0 µF | 10 µF | 6,19 V | 5,71 V |
| 4,0 µF | **22 µF** | **5,76 V** | 5,33 V |
| 0,9 µF | 22 µF | 5,34 V | 5,14 V |

- **Hace falta capacidad cerámica de baja impedancia.** Un electrolítico agregado **no sirve**: con
  ESR de 1,5–8 Ω el pico queda en 6,3–7,0 V, porque el golpe dura microsegundos y lo que hace falta
  es capacidad de baja impedancia, no pérdidas. Fue una intuición mía que la simulación desmintió.
- **Con qué pieza:** sirve el 22 µF/16 V X5R 0805 (`C45783`, ya en el BOM). A 5 V un X5R 0805 suele
  perder 40–60 % por tensión continua, así que **probablemente hagan falta dos**. ⚠️ Hay que
  verificar la curva del fabricante de `C45783`.
- **El costo:** sube la capacidad que ve el USB en sólo-USB, de 48 a ≈70 µF. Es un problema sólo
  de norma (ver arriba).
- **Antecedente:** el nodo equivalente de la DS11 tenía 11,2 µF, y la DS12 quedó con 1,1 µF.
- **Achicar C55 a 1 µF** ayuda poco sola: el peor caso queda en 6,71 V. No puede bajar de 1 µF,
  porque la ECN *Device Capacitance* de USB 2.0 exige CRPB ≥ 1 µF.

---

## 4. Qué cambió en mis propias conclusiones con la refutación

Para que quede escrito qué afirmaciones mías no sobrevivieron:

1. **S1:** el límite de USB era **100 mA**, no 500. Y la DS11 era ilustración, no prueba.
2. **S2:** la prueba en modo diodo, tal como la escribí, daba un falso positivo. También estaba mal
   citado el §6.3.16 del STM32: el de la corriente de fuente es el §6.3.15.
3. **S4:** el "peor caso" de sobretensión en sólo-USB no era el peor (5,04 → 5,45 V). Y la afirmación
   de que con el 20 V presente no pasaba nada **era falsa** (S4.f).
4. **Control de calidad:** el dato plantado (C60 "cerámico") se detectó. F2 y F3 se confirmaron como
   PTC (`C523825` = SMD1206P110TF/16). El campo *description* del símbolo está copiado de F1.

---

## 5. Nuevos, aparecidos de paso (🔎 sin verificar a fondo)

| # | qué | estado |
|---|---|---|
| **N1** | **La fuga de D23 puede subir `+5V_AUX` por encima de los 6,5 V del AP2125.** Pasa con el AK5538 en power-down, que es lo que ocurre con el **STM32 en blanco** (PC7 al aire, R158 baja PDN): `+5V_AUX` queda sostenido por <0,2 mA (AP2125 60/90 µA según DS38144, AK5538 10/100 µA según su p.9). Con 15 V en reverso, D23 fuga 1,6 / 95 / 650 µA a 25 / 75 / 100 °C (MDD Fig.2, típicos; verifiqué la figura). La fuga de D19 en sentido contrario casi la compensa, pero la hoja permite **0,5 mA a 25 °C** como máximo (p.1): por especificación, no hay margen | 🟡 **plausible, próxima ronda.** Arreglo candidato: una resistencia de drenaje de `+5V_AUX` a GND, p. ej. 1 kΩ 0402 (`C11702`, ya en el BOM, 23 mW) |
| **N2** | La fuga de D8, sin cable en J8, alimenta el divisor de PA5 (R161 62 k / R160 100 k). Con ≥23 µA, PA5 pasa VIH: el STM32 "vería" un host que no está. Con piezas típicas pasa a TJ ≈ 60 °C | plausible, sin revisar el firmware |
| **N3** | U6 y U9 (USBLC6) tienen el pin 5 en VBUS. Si el pull-up de D+ queda activo con el host apagado, puede inyectar ~mA al VBUS del host por el diodo interno. Es la prueba de *Back Voltage* de la norma | plausible |
| **N4** | Con el 20 V puesto y sin cable, el VBUS de **J3** queda en ≈4,7 V por la fuga de D2: no tiene resistencia de descarga. Una fuente Type-C exige vSafe0V (≤0,8 V) antes de conectar (§4.5.2.2.8.2), y podría negarse a enumerar el ESP32 si el cable se enchufa **después** del 20 V. La DS11 tiene la misma topología en J3 | **medible hoy en una DS11** (multímetro en VBUS de J3, y ver si la Mac lo enumera). ⚠️ Si se agrega una resistencia de descarga, J3 queda expuesto a S4.f: hay que resolver los dos juntos |
| **N5** | El §6.1.a y el §12 de `DS12_ALIMENTACION.md` nombran R162 (a `+5VA`), R163 (a GND) y C327. El netlist tiene **R163 a `+5VA`, R162 a GND y C312**; C327 no existe | documento |
| **N6** | Con el firmware de hoy, reflashear entre los 15 y los 75 s del arranque cuenta como un intento fallido del autoarranque | firmware |

---

## 6. Correcciones propuestas a los documentos (sin aplicar: decide Fernando)

```diff
 DS12_ALIMENTACION.md §5, tabla de modos, renglones 2 y 3
-| ✅ | ✅ | — | 19,7 V | 3,28 V | 5,09 V | 4,8 V | idéntico; D2 en reverso, la PC no entrega nada |
+| ✅ | ✅ | — | 19,7 V | 3,28 V | 5,09 V | 4,8 V | D2 conduce si VBUS ≳ +5VA − 0,1 V: la PC entrega
+|    |    |   |        |        |        |       | de 0 a toda la carga de +5V_AUX (45 mA; con expansión,
+|    |    |   |        |        |        |       | cientos de mA). Nada vuelve hacia la PC |

 DS12_ALIMENTACION.md §6.1.a
-En la DS11 no pasaba porque `+3V3` colgaba de `+5VA` y ambos morían juntos.
+En la DS11 no pasaba con el UVLO porque `+3V3` colgaba de `+5VA`; pero en modo sólo-USB la
+DS11 tenía exactamente la misma condición (S2 de DS12_REVISION_ALIMENTACION.md).

 DS12_ALIMENTACION.md §6.1.a (dibujo) y §12
-R162 100 kΩ de +5VA · R163 100 kΩ a GND · C327 4,7 nF
+R163 100 kΩ de +5VA · R162 100 kΩ a GND · C312 4,7 nF        (netlist del 2026-10-01)

 DS12_USB_STM32.md l.186
-la PC entrega poco o nada
+la PC entrega entre nada y toda la carga de +5V_AUX, según su VBUS (S1)
```

---

## 7. Qué sigue

- **Antes de congelar el esquemático:**
  - decidir **S4.f**: capacidad cerámica en `+5V_AUX`;
  - resolver **N1**, que es la próxima ronda: el drenaje de `+5V_AUX`;
  - mirar **N4** junto con S4.f.
- **Antes de la primera DS12 por USB:** la guarda de firmware de **S2**.
- **Mediciones que ya se pueden hacer en una DS11**, todas sin encender amplificadores:
  - S2 (i) y (ii), con el protocolo corregido;
  - N4, con el multímetro en el VBUS de J3 y la Mac;
  - S1, con un medidor USB en línea.
- **Del plan quedan** S3 y S5–S12. S5 (el AP63301 en modo USB) ya tiene media respuesta favorable en
  el §2: arranca con ≤3,7 V y tiene modo de baja caída.

---

## 8. Ronda 2 (2026-10-02) — parcial

> Se cortó a mitad por presupuesto. Lo de esta sección lo calculé y verifiqué yo, pero **no pasó por
> refutación a ciegas**: el refutador y los cuatro analistas se detuvieron antes de terminar. Lo que
> falta está en [DS12_REVISION_ALIMENTACION_RONDA2.md](DS12_REVISION_ALIMENTACION_RONDA2.md).
> **Precondición verificada:** el esquemático no cambió respecto del netlist del 1-oct (0 diferencias).

| | resultado | cómo se verificó |
|---|---|---|
| **A1 · capacitores en `+5V_AUX`** | 🔧 **Superado por el §9: son cinco.** **Tres `C45783`, no dos.** `C45783` es el Samsung CL21A226MAQNNNE (22 µF, **25 V**, X5R, ±20 %; el símbolo dice 16 V). A 5 V conserva 10,7 µF y a 7 V, 7,6 µF. Con la peor pieza (−20 %, 85 °C), el pico del golpe de ariete da 5,95–6,06 V con dos, **5,73–5,77 V con tres** y 5,58 V con cuatro. Con VBUS de 5,5 V, tres dan 5,94 V y cuatro 5,76 V | curva de Samsung (weblib, 25 °C, 120 Hz) en `revision_alimentacion/C45783_dcbias.json`; `a1_ncaps.py`, con capacidad no lineal; autoprueba contra s4f_verif y s4f_mitig |
| A1 · efecto colateral | En modo sólo-USB la caída del hub sube de 1,06 a 1,60 V. Es un tema de norma, sin daño | `s4_hotplug.py` |
| A1 · C55 más chico | No: la ECN «Device Capacitance» exige ≥ 1 µF en el pin VBUS en todo el rango, y con la placa alimentada ese pin sólo ve a C55 | ECN de USB 2.0 |
| **A2 · drenaje de `+5V_AUX`** | **1 kΩ 0402 (`C11702`)**: cubre el máximo especificado de fuga de D23 hasta más de 125 °C, con 25 mW. Un 10 kΩ (`C25744`) alcanza hasta 89 °C con 2,5 mW | `a2_drenaje.py`: Fig. 2 del SS34 digitalizada y el máximo de la p.1 |
| A2 · alternativas | Peores. Un clamp de `+5V_AUX` a `+5VA` anula el bloqueo de D19 en modo sólo-USB. Un diodo PN en D23 se come el margen de ese modo | razonamiento sobre el netlist |
| **A4 · jumper en PE15** | **Sirve, con 1 kΩ a GND** (`C11702`): en el BOM no hay resistencias de 0 Ω. PE15 es la pata 45, FT_h. Sus funciones alternativas son TIM1_BKIN, FMC_D12, COMP_TIM1_BKIN, LCD_R7 y EVENTOUT, y ninguna está en uso. Con el pull-up de 30–50 kΩ lee ≤ 0,11 V, contra un VIL de 0,99 V | DS12110 Rev 10, Tablas de pines y 60; `.ioc` y `pin_definitions.h`; grep del BOM |
| **N6 · autoarranque y reflasheo** | Arreglo mínimo: al bootear, si quedó un intento pendiente y RSR trae **SFT** sin POR, descontarlo. El reflasheo del Makefile (`mode=UR` y después `-rst`) y el OTA (`NVIC_SystemReset`) dejan SFT. Una caída del riel hace que el MAX809 baje NRST, y eso deja sólo **PIN**, que sigue contando. **No** sirve excluir PIN | `firmware/Makefile` l.480–487, `main.c` l.2093–2176 y 3077 |

**Documentos corregidos** (sólo los hechos verificados de la ronda 1):
- `DS12_ALIMENTACION.md`: §5, §6.1.a, §11.3, §12 y una nota de revisión;
- `DS12_USB_STM32.md` l.186;
- `docs/presupuesto_potencia.md`: el banco a 12 V no sirve con la DS12;
- `DS12_CHECKLIST.md`: se tildaron PC1, FL1 y C315/C320;
- el `README.md` de `hardware/DS`, y uno nuevo en `revision_alimentacion/`.

---

## 9. Ronda 2 completa (2026-10-07)

> Cuatro analistas y dos refutadores a ciegas, cada refutador con un dato falso plantado como control.
> Los dos datos plantados se detectaron: un 0 Ω «C17168» que no está en el BOM, y un C31 «de 10 µF»
> que en realidad es de 4,7 µF. Los informes y scripts de cada uno, con autoprueba, están en
> [`revision_alimentacion/ronda2/`](revision_alimentacion/ronda2/).
>
> **Esta sección corrige al §8:** ahí decía «tres C45783», y no alcanzan.

### 9.1 Lo que hay que dibujar

| # | cambio | piezas (todas en el BOM) | por qué | estado |
|---|---|---|---|---|
| 1 | **Cinco `C45783`** de `+5V_AUX` a GND | 5 × `C45783` (22 µF, 25 V, X5R, 0805) | El golpe de ariete de J8 (S4.f) en el peor caso combinado: +5VA alto, C55 vacío, capacidad × 0,9 por frecuencia y envejecimiento, pieza al −20 % y a 85 °C. Da 6,09 V con 3, 5,87 V con 4 y **5,73 V con 5**. Ninguno pasa los 6,5 V absolutos del AP2125. Con 4 queda debajo de los 6,0 V de operación pero arriba del criterio de 5,8 V. Alternativa: 3 `C45783` y C55 de 2,2 µF (`C23630`) dan 5,76 V, si `C23630` cumple la ECN de ≥ 1 µF a 5,25 V (**ABIERTO**) | refutado dos veces: se pasó de 3 a 5 |
| 2 | **1 kΩ de `+5V_AUX` a GND** | `C11702` | La fuga de D23 con el STM32 en blanco (N1). Cubre la fuga típica hasta ~128 °C; la máxima en caliente no está especificada | confirmado |
| 3 | **1 kΩ de cada red VBUS (J3 y J8) a GND**, del lado del conector | 2 × `C11702` | Con 20 V y sin cable, la fuga de D2/D8 deja el VBUS en 4,8 V en J3, y en J8 desde ~60 °C. Una fuente USB-C exige < 0,8 V (vSafe0V) para conectar. Además, en J8 PA5 cree ver un host que no está (N2). Con 1 kΩ: 0,17 V (fuga típica) / 0,40 V (máxima) a 85 °C. **Sólo funciona junto con el 4** | confirmado |
| 4 | **El pin 5 de U6 y de U9 (USBLC6) a `+3V3`**, no a VBUS, con un 100 nF junto a cada uno | 2 × `C1525` | Sin esto, el pull-up de D+ empuja corriente por el diodo interno del USBLC6 hacia VBUS: la red queda en 1,3–1,5 V aun con el 1 kΩ, y alimenta al VBUS de una PC apagada (N3). La hoja del USBLC6 (Fig. 15) admite el pin 5 en +VCC | confirmado, con correcciones |
| 5 | **C28 (EN del ESP32) de 0,1 µF a 1 µF** | `C15849` | El arranque suave del AP63301 sube `+3V3` en ~4 ms. Con 0,1 µF el RC del EN sólo tolera rampas de 0,66 ms (criterio estricto) a 4,6 ms (laxo); con 1 µF, de 7 a 46 ms. Espressif pide 10 kΩ + 1 µF (hoja del módulo, p.25) | confirmado |
| 6 | **TDMIN (pin 35 del AK5538) a GND** | — | La hoja dice «TDMIN, TEST: Connect to DVSS» en modo PCM. Hoy anda por un pull-down interno de 100 kΩ | confirmado, gravedad baja |
| 7 | **Jumper de identidad: PE15 a GND por 1 kΩ** | `C11702` | En el BOM no hay 0 Ω. Ojo: el firmware pone PE15 en analógico en `main.c:4558–4562`, así que hay que leerlo antes | confirmado |
| — | opcional: C73–C80 (8 × 10 µF de **6,3 V** sobre `+5VA` de 5,23 V, margen del 17 %) | — | Vienen de la DS11, que anda. Reemplazarlos cambia el footprint | decisión |
| — | opcional: R20 de 33 Ω a 5,1 kΩ (S2) | `C23186` | Depende de la medida M5 | después de medir |

### 9.2 Lo que hay que hacer en el firmware (antes de la primera DS12 por USB)

1. **Bajar SD con el *analog watchdog* del ADC sobre PC1**, a 2,25 V. Revisarlo cada 1 ms no alcanza: con señal, entre 4,58 V y el UVLO de los PAM pasan 0,78 ms. Con eso se evitan la violación de S2 y el pop al apagar (S10). El autoarranque, `/amps`, K2 y `/autostart` tienen que esperar a que PC1 esté sobre 2,25 V, y el autoarranque tiene que **esperar en AS_WAIT**, no rechazar. Todo esto sólo con `BOARD_HAS_5VA_SENSE`: en la DS11, PC1 está al aire.
2. **Autoarranque (N6).** Una marca propia, en `RTC->BKP3R`, que se ponga al empezar un intento y se borre en AS_DONE. Al arrancar, si la marca está y el reset fue SFT sin POR, no se cuenta como falla. Con el contador solo no alcanza: un OTA después de AS_GAVEUP rearmaría el encendido. Además, los errores esporádicos reinician la ventana de 60 s sin límite: un nodo con underruns ocasionales puede quedar mudo en tres noches.
3. **PA5**: entrada sin pull, leída en digital. El USB del STM32 se conecta sólo si PA5 lleva ≥ 20 ms en alto, y se desconecta apenas baja.
4. **ESP32**: el decodificador de RSR usa el bit 19 para POR, que es el 23 (el 19 es D1RSTF), y el 25 para WWDG, que es el 28. Está en `firmware_esp32c3/main/main.c`, en `reset_name()`, y hoy acierta por casualidad. Opcional: subir el detector de brown-out al nivel 4 (≈2,92 V): el ESP32 no tiene supervisor y hoy corre fuera de su hoja entre 2,5 y 3,0 V.

### 9.3 Lo verificado que no cambia el dibujo

- **Ruido hacia el ADC (S11): 🟢 con los capacitores de `+5V_AUX`.** Hoy FB69 y C57 resuenan a 172 kHz, con Q de hasta 4,3 si la expansión está cargada. Con 22 µF o más esa resonancia baja a 28 kHz, con Q ≤ 0,7, y aporta −39 dB a 250 kHz. Llega al ADC por debajo de −172 dBFS/Hz. El RC de VREFH es el de AKM.
- **Apagado (S10): 🟢.** El diodo de cuerpo de IC4 no llega a conducir (refutado). `+3V3` dura 4,6–8,3 ms más que `+5VA`. Lo único que hay que cuidar es el firmware del punto 9.2-1.
- **Lo heredado del artifact**, corregido:
  - El arranque del buck A carga 1,3 mF (hasta 1,6) con 2,3–3,0 A, contra un límite de al menos 4,0 A.
  - El cruce del lazo queda **ABIERTO**: estimo 1,3–2,6 kHz con 35–64° de margen de fase, pero la hoja no publica la compensación.
  - El UVLO enciende entre 13,6 y 14,9 V y apaga entre 10,2 y 12,1 V. La ventana de entrada conviene escribirla **16–21 V**.
  - El MAX809S suelta el reset entre 2,78 y 3,08 V.
  - A 24 V, el TVS puede conducir y abrir F1, según la pieza.
  - El DFU es desaconsejable, pero no por el cristal.
- **Matriz de cobertura:** 176 pines de alimentación; ninguno al aire ni en la red equivocada. En desacople hay diferencias menores con las hojas: VDDA/VREF+ tiene un solo 100 nF; los PAM, un 1 µF cada dos chips.
- **S6** (ferrites): sin saturar. **S9** (PG con VIN = 0): dentro del máximo. **C4**: la predicción es que la entrada del ADC se asienta en **1,65 V (1,55–1,75)**; lo cierra la medida M4.

### 9.4 Lo que queda ABIERTO, con la medida que lo cierra

Todo está en [`revision_alimentacion/medidas_ds11.md`](revision_alimentacion/medidas_ds11.md):
- **Modo sólo-USB con 0,9 A:** `VIN_B` podría bajar a 3,2–3,4 V, por debajo del arranque del AP63301. Se mide en la primera DS12.
- **Estabilidad del AP2125** con cerámicos (M7).
- **PSRR del PCM1690** (M11).
- **El MAX809S de UMW contra el reset del STM32**, porque su salida es push-pull. Se prueba con el primer OTA de la DS12.
- **La rampa real de `+3V3`.**
- **Si `C23630` cumple la ECN.**

### 9.5 Dos cosas que faltaba razonar (2026-10-08)

- **Pasar el pin 5 de U9 a `+3V3` le saca al VBUS de J8 el recorte del USBLC6.** No pasa nada:
  - Simulé el golpe de ariete sin ese recorte, con los cinco `C45783` en su peor caso y VBUS de 5,5 V.
  - El VBUS de J8 llega a **6,39 V**. C55 y F3 son de 16 V y D8 de 40 V.
  - PA5 queda en 3,40 V. El pico apenas la sube: C50 (100 nF) con el divisor hace una constante de 3,8 ms, y el pico agrega ≤ 7 mV. El límite del pin (TT) es 4,0 V.
  - `s4f_verif.caso()` con `rd_clamp=0`.
- **El modo sólo-USB con 0,9 A (N-b del analista de lo heredado) no deja la placa sin arrancar.** El AP63301 arranca con poca carga.

  | caso | ~0,15 A (arranque) | 0,9 A (ESP32 transmitiendo) |
  |---|---|---|
  | típico (Mac a 5,05 V, cable de 0,15 Ω, F2 de 60 mΩ) | `VIN_B` 4,31 V | 3,98 V |
  | peor apilado (puerto de 4,75 V, cable de 0,5 Ω, F2 ya disparado, 210 mΩ) | 3,94 V | **3,23 V** |

  - **Al arrancar**, 3,94 V queda arriba de los 3,7 V de arranque máximo (DS42002 p.4).
  - **Con TX**, el chip se apaga recién ≈0,44 V más abajo (3,06 V típico). En baja caída, `+3V3` queda en 3,13 V: arriba del MAX809S (≤ 3,08 V) y de los 3,0 V del ESP32. La placa sigue andando, con menos margen.
  - **El único caso feo** es que todo se apile *y además* el umbral de apagado del AP63301 esté en su extremo alto, que la hoja no publica.
  - **Regla de banco:** para flashear sólo por USB, usar la Mac directo con un cable corto, o tener la fuente de 20 V puesta.
  - Severidad 🟢. Se confirma con la medida de C319 en la primera DS12.
