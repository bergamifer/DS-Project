# Ronda 2 · A5 completitud + S6–S9, pista del EN y C4

7-oct, sólo esquemático. Scripts con autoprueba: `a5_matriz.py` (8 positivas, 4 negativas: TVDD a masa, VDD de un PAM
al aire, VCAP sin capacitor) y `en_rampa.py` (contra el analítico; el criterio invertido falla). Matriz completa en `matriz.md`.

## 1. Matriz de cobertura

**Pines (176): 175 OK ahora y 1 verificado antes. Ninguno al aire ni en la red equivocada.** Cubre AK5538, PCM1690, STM32,
ESP32, los 32 PAM, CAT24C256, PRTR5V0U2X y USBLC6; ningún pin con nombre de alimentación quedó sin mirar. Ya
verificados antes: IC4, U11, U10, Q1, IC1, D9, D3, los SS34, J8/U9/FL1, J3, F1–F7 y S1/S2/S4.

No existen: VCOM en el AK5538 (p.4–6); VDDLDO (atado a VDD, nota 8) y VDD33USB (columna LQFP100 «–») en el STM32.
Que VDD33USB vaya a VDD la hoja no lo dice: **ABIERTO menor** (analista de USB).

**Desacople: 19 de 37 grupos OK** (AK5538 entero, PCM1690 entero, VDD y VCAP del STM32, 3V3 del ESP32). Los PROBLEMA:

| chip | red | hay | pide la hoja | severidad |
|---|---|---|---|---|
| U5 ESP32 | EN | 10 kΩ + **0,1 µF** | 10 kΩ + **1 µF** (MINI-1 v1.6 p.25) | 🟡 (ver la pista) |
| U7 STM32 | VDDA+VREF+ (una sola red) | 1 µF + 10 µF + **un** 100 nF | 100 nF + 1 µF en cada pin (DS12110 Fig.15) | 🟢, depende del PCB |
| 16 bancos de PAM | `+5VA` | **un** 1 µF cada dos chips | 1 µF por chip (DS41333 p.7) | 🟢, igual que la DS11 |

## 2. Capacitores con margen insuficiente

Revisé 113 capacitores, de 15 códigos, cada código contra la página de LCSC o JLCPCB. 103 están OK.

| C | código | nominal | red: máximo | margen |
|---|---|---|---|---|
| **C73–C80** | C1691 (Samsung CL10A106MQ8NNNC) | **6,3 V**, X5R 0603, 10 µF | `+5VA`: 5,23 V (5,75 V si actúa el OVP) | **17 %** (9 %) |
| C273, C314 | C2836437 | 35 V, aluminio | `+VIN_LOCAL`: 21 V / 32,4 V | 40 % / **7 %**, sólo en el pico del TVS |

C73–C80 son iguales en la DS11; a 5 V, un X5R de 6,3 V pierde gran parte de su capacidad (cuánta: ABIERTO, sin curva
del fabricante). Arreglo posible: `C45783`, ya en el BOM. C273/C314: sobretensión admitida sin hoja, **ABIERTO menor**.

**Texto inconsistente:** C67, C69, C70, C321 y C322 dicen «16V» en value y BOM; `C45783` es de **25 V**. La pieza está
bien; el riesgo es que alguien la «corrija».

## 3. Veredictos

**S6 · ferrites (FH CBW201209U601T) 🟢.** La hoja (p.5) da DCR ≤ 0,22 Ω e Ir = 1,5 A, **definida como 40 °C de
calentamiento**. No trae curva de impedancia contra corriente: **ABIERTO**. Pero eso no importa para el audio. La curva
sin corriente (p.24) da pocos Ω a 1 MHz: a 250–450 kHz el ferrite no filtra nada ni siquiera en reposo, y para S11 no
hay que contar con él. Corrientes:
- FB69: ≤ 1,06 A (AK5538, drenaje y F6+F7) → cae 0,23 V y disipa 0,25 W, debajo de Ir.
- FB3: 4 × 74 mA típ. / 110 mA máx. (PCM1690 p.8) = 0,30–0,44 A → cae ≤ 0,1 V; `+5VA_DAC` queda ≥ 4,85 V, sobre el
  mínimo de 4,5 V.
- FB1: cae ≤ 11 mV.

**S7 · AP2125: ABIERTO 🟡.** Único gráfico de ESR (p.15): AP2125-1.8 con COUT = 1 µF, zona estable entre ≈0,2 Ω y
100 Ω, sin capacidad máxima. `+3.3VA` tiene ≈47 µF de cerámicos, con ESR de mΩ:
debajo de ese piso. En contra, la p.1 dice «Compatible with Low ESR Ceramic Capacitor», y la DS11 tiene exactamente la
misma población. Se cierra con un osciloscopio en TP2 de una DS11 (acople AC, 20 MHz), buscando oscilación sostenida.

**S8 · MAX809S 🟡 bajo.** El MAX809S resetea **sólo al STM32**. En el netlist, NRST = IC1, U7.14, J7.6, K4, C36 y C52;
EN = U5.8, R8, C28 y K1. Datos de UMW que no estaban: sube a 2,93 V (2,78–3,08 V en temperatura), **baja al 95 % de eso
(≈2,78 V)**, el retardo es de 150–560 ms y la salida es push-pull (p.6). El ESP32 pide 3,0–3,6 V y **no tiene
supervisor**: en una caída de tensión sólo lo resetea su detector interno, en nivel 7 ≈ **2,51 V** (`sdkconfig`; el
Kconfig de ESP-IDF v6.1 dice que son «estimates»). El RC del EN no ayuda: al bajar el riel el capacitor deja a EN
igual o arriba de VDD. Entre 2,51 y 3,0 V el ESP32 corre fuera de hoja; Espressif recomienda un supervisor de «around
3.0 V». Opción de firmware: nivel 4 (2,92 V).

**Pista del EN: PROBLEMA 🟡.** Rampa de `+3V3` más larga con la que el RC todavía cumple (`en_rampa.py`):

| RC | estricto (EN bajo hasta 50 µs después de 2,8 V; ESP32-C3 Fig.2-4) | laxo (EN no alto con VDD < 3,0 V) |
|---|---|---|
| 10 kΩ · 0,1 µF (hoy) | 0,66 ms | **4,3 ms** |
| 10 kΩ · 1 µF | 7,1 ms | 43 ms |

En la DS11 `+3V3` subía en ≈1–2 ms; en la DS12 manda el arranque suave del AP63301: **4 ms típicos, sin mín/máx**
(DS42002 p.6), en el borde del laxo (con VDD = 3,0 V, EN ya está en 2,2 V). Igual en sólo-USB y en cada reinicio por UVLO. Arreglo: C28 → 1 µF (`C15849`, ya en el BOM). El
NRST no tiene este problema: el MAX809S sostiene el reset 150 ms con cualquier rampa.

**S9 · PG con VIN = 0 🟢.** En la tabla de máximos (DS41949 p.5), las tensiones referidas a otro pin lo dicen (VBST
respecto de VSW, VSW respecto de VIN). VPG dice sólo «−0,3 a +6,0 V», o sea respecto de GND. El pull-up interno de 5 MΩ
va al VCC interno (diagrama de bloques, p.3). Con VIN = 0 entran ≤0,65 µA y PG queda en ≈3,24 V. Ninguna mención de PG
habla del chip apagado. Lo funcional ya está escrito: PG dice «bien» sin `+5VA`. Dato nuevo para la fila del UVLO: la Fig.9 (p.8, curva típica) muestra PG en bajo con VIN = 12 V y EN bajo.

**C4 · entradas del ADC (sólo material).** El netlist confirma la cadena: J9 → 2,2 µF (C309) → 470 Ω (R130) → AIN1P,
con 100 pF (C281) a masa. Entre el capacitor y el pin no hay ningún camino de continua. La DS11 tiene la misma cadena.

La hoja no dice adónde va la resistencia de entrada de 8,8–12 kΩ (p.9); la AK5558 tampoco. Indicios de que va a un
punto interno ≈AVDD/2: se duplica en DSD64 (capacitores conmutados); la p.69 dice que S/(N+D) depende de la «DC bias
current» y pide 0,48–0,52 × AVDD; en power-down los pines son Hi-Z (p.5).

**Predicción:** con PDN alto y MCLK presente, el pin queda en **1,65 V, entre 1,55 y 1,75 V** (AVDD de 3,234 a
3,366 V, AP2125 p.8). Llega ahí con τ ≈ 24 ms, y un tester de 10 MΩ le agrega un error de apenas 0,1 %. **La
refutaría** una lectura que se va sola hacia 0 V en decenas de segundos: querría decir que no hay camino interno. Con
PDN bajo, el valor no está definido.

## 4. Problemas nuevos

1. **C73–C80 de 6,3 V** (tabla 2).
2. **El MAX809S de UMW da «VDD Rise Rate 100 V/s» como máximo absoluto (p.5).** La rampa de 4 ms equivale a 825 V/s.
   Probablemente sea un error de unidades; no lo pude contrastar con onsemi porque se cortó el acceso web. **ABIERTO.**
3. **Salida push-pull sobre un NRST bidireccional.** Para que funcionen `NVIC_SystemReset` (el OTA) y el IWDG, el STM32
   tiene que poder bajar el pin contra esa salida (DS12110 p.145, nota 2). Con el Maxim de la DS11 funciona; con el de
   UMW, **ABIERTO**. Primera prueba en la DS12: que el OTA reinicie la placa.
4. **USBLC6-2: VBR(VBUS–GND) ≥ 6 V** (p.2). En el enchufe en caliente de J8, U9 conduce arriba de 6 V. Hay que ver si la
   simulación de 7–8,5 V lo tuvo en cuenta (analista de USB).
5. Fuera de alimentación 🟢: TDMIN (pin 35 del AK5538) está al aire y la p.6 pide llevarlo a DVSS. K1, rotulado «BOOT
   ESP32», está en EN: es un reset.

Depende del PCB: qué tan cerca quedan los 1 µF de los PAM y el 100 nF de VDDA/VREF+.

## Archivos

`.ds12_audit/r2/completitud/`: `a5_matriz.py`, `en_rampa.py`, `matriz.md`, tres CSV, `hojas_por_ref.json`, `informe.md`.
`.ds12_audit/hojas/`: siete `.txt` nuevos (ESP32-C3 serie, ferrite, MAX809S UMW, PRTR5V0U2X, USBLC6-2, CAT24C256,
AK5558), los PDF en `dl_*` y los PNG de las curvas.
