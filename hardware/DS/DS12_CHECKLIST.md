# DS12 — la lista para tildar mientras dibujás

> **2026-09-11.** Esto no reemplaza a ningún documento: es el índice para no tener que
> leer tres archivos en paralelo mientras se dibuja. Cada línea dice **qué poner** y
> **dónde está el porqué**. Si una línea y el documento se contradicen, **gana el
> documento** — y hay que corregir esta lista.
>
> **Estado 2026-09-29** — tildado contra el esquemático de `wip/ds12-dibujo` (456f578) con
> `tools/sch2net.py`, el netlist derivado del `.kicad_sch` (0 diferencias contra la exportación
> de KiCad de la DS11). Lo tildado está **dibujado y verificado pin por pin**; lo que no, no.
> El detalle y lo que falta antes de rutear: artifact «Cambios para DS12».
>
> Orden: el de dibujo, no el de importancia.
>
> - Fuente → [`DS12_ALIMENTACION.md`](DS12_ALIMENTACION.md) (citado como **AL §x**)
> - Auditoría → [`DS12_AUDITORIA.md`](DS12_AUDITORIA.md) (**AU §x**)
> - Backlog → [`DS12_PLANNING.md`](DS12_PLANNING.md) (**PL §x**)

---

## A. La etapa de fuente

### A.1 Entrada y protección — AL §3.1

- [x] **F1** — fusible 1206 **2 A / 63 V**, lento, `C136345`. Va en `+24V` → `VIN_PROT`.
      Es de un solo uso a propósito: no hay PTC de 1206 con ≥2 A de hold y ≥30 V, **y el
      fusible aporta la impedancia serie que el TVS necesita para coordinar**.
- [x] **Q1** — P-MOSFET **AO3401A** `C15127` (Basic de JLCPCB). Drain a `VIN_PROT`,
      source a `VIN_LOCAL`.
- [x] **R1** — 100 kΩ de gate de Q1 a GND (lo enciende). *(Esquemático: R149.)*
- [x] **DZ1** — zener **10 V** `C173431` entre gate y source, cátodo al source. *(Esquemático: D21.)*
      🔴 **10 V y no 12 V**: el AO3401A tiene VGS máx ±12 V y un zener de 12 V llega a
      12,7 V por tolerancia — el componente que protege la compuerta la destruiría.
- [x] **D8'** — **SMBJ20A** `C364296` de `VIN_LOCAL` a GND. *(Esquemático: **D9**, con símbolo bidireccional A1/A2: la polaridad la fija la huella, pin 1 en `+VIN_LOCAL`.)* Reemplazo directo sobre el
      footprint `D_SMB` que ya existe: cambia el part number, no el dibujo.
- [x] **C_bulk** — **100 µF / 35 V electrolítico, ESR 600 mΩ**, `C2836437`. *(Esquemático: C273 **y** C314, dos.)* El footprint
      D6,3×7,7 ya está en la placa (es el de los 220 µF). **La ESR alta es lo que se está
      comprando**: sin pérdidas no hay amortiguación del hot-plug. No sustituir por
      polímero ni por cerámicos.
- [ ] **C_in1** — 22 µF / 50 V X7R 1210. 🔧 **29-sep: no está**; en su lugar hay un segundo 100 µF electrolítico (C314). Decidir si va.
- [x] **El orden importa**: conector → fusible → P-FET → TVS → bulk. Con el TVS *después*
      del FET, la polaridad invertida apaga el FET y no quema el fusible.

### A.2 Buck A — `+5VA`, el riel de los amplificadores — AL §3.2

**IC2 = AP63356DV-7**, LCSC **`C2157973`** ⚠️ **no** `C2158014` (ése es el AP63357, versión
PFM).

- [x] **VIN** — 2× **10 µF / 50 V X7R 1210** (`C138687`) + 100 nF, pegados al pin. *(C316 + C324, C315.)*
      ⚠️ El 10 µF que hay hoy en la placa (`C1691`) es **6,3 V en 0603**: sobre 20 V no
      filtra poco, **se destruye**.
- [x] **EN** — divisor de UVLO: **R3 470 kΩ** (`C23178`) a `VIN_LOCAL`, **R4 40,2 kΩ**
      (`C2933097`) a GND → **VON 14,3 V / VOFF 11,1 V** *(R151 / R156)*. Los dos valores ya están en la
      placa (R33 y R34).
      ⚠️ Esto **deja fuera el banco a 12 V**: con 12 V de entrada los amplificadores nunca
      encienden (el riel digital sí arranca). Si querés poder trabajar a 12 V, cambiá
      **sólo R4** a 60,2 kΩ — ver la tabla de AL §3.2.
- [x] **COMP → GND** (compensación interna; sin RC externo).
- [x] **PG** → `PWR_GOOD_5V`, con **100 kΩ** (`C25741`) a `+3V3`, a un GPIO libre del STM32. *(Esquemático: el net se llama `PG`; R159 es el pull-up y R152 la serie hacia PA10.)*
      **Este net lo usa también el BAT54C del punto A.8: no es sólo telemetría.**
- [x] **BST** — 100 nF (`C1525`) a SW. *(C317.)*
- [x] **SW** → **L1 6,8 µH, Isat 9,6 A**, `C408485` (10×11 mm, `Inductor_SMD:L_Sunlord_MWSA1004S`).
- [x] **FB** — **R1 40,2 kΩ** (`C2933097`) de `+5VA`, **R2 7,5 kΩ** (`C23234`) a GND →
      **5,088 V** *(R150 / R153)*. Es literalmente el divisor de la DS11 (R34/R32).
- [x] **Cff 220 pF** (`C39122`) de `+5VA` a FB. *(C318.)*
- [x] **Salida** — 2× 22 µF/16 V 0805 (`C45783`) + los 5× 220 µF que ya existen. *(C69 / C70 + C71, C290–C293.)*
- [ ] **Vías térmicas bajo el pad** (V-DFN3020). Sin ellas los 25 °C/W del datasheet no
      existen — ese número es con 4 capas y 2 oz.

### A.3 Buck B — `+3V3`, el riel digital (el integrado que faltaba) — AL §3.3

**IC4 = AP63301WU-7**, LCSC **`C2158003`**, TSOT-26. Segunda fuente pin-compatible si
alguna vez no hay: AP63201WU-7 (`C2071044`).

- [ ] **VIN** (`VIN_B`) — 2× **10 µF / 50 V X7R 1210** + 100 nF pegados al pin. 🔧 **29-sep: hay uno solo** (C319), más C60 (22 µF electrolítico) y C320 (100 nF). El net se llama `IN`. Decidir si va el segundo 1210.
- [x] **EN → VIN_B directo, sin divisor.** Tiene que arrancar también con ~4,1 V en modo
      USB: si le ponés UVLO, se pierde el modo de flasheo sin fuente.
- [x] **BST** — 100 nF a SW. *(C61.)*
- [x] **SW** → **L2 4,7 µH**, Isat 7,2 A, `C408447` — el MWSA0603S-4R7MT de la DS11 (AL §3.3, 🔧 2026-09-28).
- [x] **FB** — **R1 62 kΩ** (`C37825`, **el único valor de resistencia nuevo de todo el
      bloque**) de `+3V3`, **R2 20 kΩ** (`C25765`, = R31) a GND → **3,280 V** *(R155 / R154)*.
- [x] **Cff 100 pF** (`C327198`, C323) en paralelo con R1 — **no 220 pF**. Con 220 pF el par
      cero-polo queda 2,6× por debajo del diseño de referencia y come margen de fase justo
      donde pega el escalón de 100→350 mA del ESP32 al transmitir.
      *(El 100 pF ya está en la placa: C274-C289.)*
- [x] **Salida** — 2× 22 µF/16 V 0805 + 100 nF. *(C321 / C322.)*

### A.4 El OR de fuentes y `+5V_AUX` — AL §3.4

- [x] **Renombrar el net `+5V` a `+5V_AUX`.** Ya no es el riel principal, y el nombre viejo
      invita al error.
- [x] **Los cinco diodos del OR pasan a SS34** (`C8678`, SMA 40 V/3 A): D9, D2, D11, y los
      dos nuevos **D20** (`VIN_LOCAL` → `VIN_B`) y **D21** (`+5V_AUX` → `VIN_B`).
      ✓ Usan `Diode_SMD:D_SMA`. *(Esquemático: D19 + FB69 desde `+5VA`, D2, D11, D20 → `IN`, D23 → `IN`; D8 pasó a ser el del USB del STM32.)*
- [x] **D20 y D21 no son opcionales**: sin D20 los 5 V del USB salen por J11/J12 a **todas
      las placas de la cadena**; sin D21 los 19,7 V de `VIN_B` vuelven al AP2125, cuyo
      máximo absoluto de entrada son **6 V**.
- [x] **F2 y F3** — PTC en los dos VBUS (USB del ESP32 y USB del STM32): **SMD1206P110TF/16,
      `C523825`**, 1,1 A hold / 2,2 A trip, footprint 1206 (el mismo que el fusible). 🔧 Decidido
      el 2026-09-28: el de 750 mA quedaba debajo del pico real de 900 mA. AL §3.4.
- [x] **D2, D8 (USB del STM32) y D11 (ST-Link) a SS34** (`C8678`, `D_SMA`): con B5819W el
      modo sólo-USB se queda sin margen. El de `+5VA` → `+5V_AUX` puede seguir B5819W. AL §3.4.

### A.5 Riel analógico `+3,3VA` — AL §3.5

- [x] **U10 = AP2125K-3.3TRG1 (`C110463`) se queda igual**, desde `+5V_AUX`. **No tocar.**
      Lo que cambia es el contexto: su entrada pasa de 800 mA a ~60 mA y FB2 vuelve a
      filtrar de verdad.
- [x] **U11 (AMS1117-3.3, `C6186`) SE ELIMINA.** *(El designador U11 lo ocupa ahora el AP63301.)* Es el único componente de la cadena que no
      cerraba, y con él se van 1,2 W de calor.
- [ ] *(Recordatorio: `+3,3VA` **no** puede salir de `+3V3` con un ferrite. Un ferrite a
      1 kHz es una resistencia de 220 mΩ — 0 dB de rechazo en banda audible — y además un
      LDO necesita caída para regular. Del otro lado están los cuatro VREFH del AK5538, que
      son el fondo de escala del ADC.)*

### A.6 Supervisor — AL §3.6

- [x] **IC1: MAX809T → MAX809S** (`C347371`, umbral 2,93 V). Reemplazo directo sobre el
      SOT-23-3 que ya existe: cambia el part number, no el dibujo.

### A.7 J7 (ST-Link) — AL §3.7 y PL §2b

- [x] **`+3V3 ─ 100 Ω (C177347) ─ J7.1`.** *(R31.)* Va **resistencia, no Schottky**: ese pin es
      VTref, una entrada de sensado. El diodo resuelve el backfeed pero deja intacto el modo
      de falla que de verdad ocurrió — la sonda intentando entregar 700 mA.
- [x] **D11 en el pin 10 se queda como está.**

### A.8 🔴 Los dos modos de falla que **crea** este rediseño — AL §6.1

El UVLO del buck A inventa un estado que la DS11 nunca tuvo: el dominio analógico se apaga
y vuelve **mientras el digital nunca se cae**. Dos cosas dependían de que todo muriera junto.

- [x] **PDN_AK**: PC7 lo maneja **a través de 10 kΩ** (`C25744`), y un **BAT54C** (`C2135`,
      cátodo común) con el **ánodo (pin 1) en `PDN_AK` y el cátodo (pin 3) en `PWR_GOOD_5V`**.
      🔴 *La versión anterior lo tenía al revés*: así el diodo quedaba en inverso justo cuando
      tenía que conducir. Cuando PG cae, arrastra PDN_AK; cuando sube, PC7 manda. Sin esto **el
      AK5538 no vuelve a resetearse nunca**. AL §6.1. *(R157 y D3 — ⚠ **D3 quedó sin LCSC**: ponerle `C2135`, y una × en su pin 2.)*
- [x] **Pull-down de `PDN_AK` a GND: 40,2 kΩ** (`C2933097`, R158), **no 10 kΩ** — con la serie de
      10 kΩ, un pull-down de 10 kΩ deja el alto en 1,65 V, debajo del VIH del AK5538 (2,30 V).
      AL §3.8 + §6.1.
- [ ] ~~`~RST` de los PCM1690 con otro diodo~~ → **en firmware** (su power-on-reset vigila VDD,
      que no se cae). AL §6.1.
- [ ] ~~`SD` de los PAM con un diodo~~ → **en firmware** por interrupción en PA10: la carrera se
      gana por milisegundos, y el diodo le sacaría 90 mA a PD3 por los 33 Ω de R20. AL §6.1.

---

## B. El resto del esquemático — AU §3

- [x] ~~**Bias DC de las 8 entradas del ADC**~~ — **no hace falta** (28-sep): los 10,4 kΩ de entrada del AK5538 fijan la continua. Queda de registro: — nodo VCM compartido (divisor 10k/10k desde
      `+3,3VA` → 1,65 V, con 10 µF + 100 nF) y **una resistencia de 22 kΩ por pin**.
      🔴 **22 kΩ, no 100 kΩ**: con 100 k el ruido térmico (~40 nV/√Hz) baja la DR a ~104 dB
      y se come los 111 dB del chip. ≈US$ 0,30.
- [ ] **J7 a un JST SH de 8 o 9 pines** — J5, J6 y J7 son hoy el **mismo conector físico**
      (mismo LCSC `C5343037`) y **J7.9 = GND mientras J5.9 = +3V3**: enchufarlo en el lugar
      equivocado pone masa contra el riel digital. Con 8 pines el error se vuelve
      mecánicamente imposible. Costo cero.
- [x] **Cuatro PTC en la expansión** (`C883126`), no dos: J5.9, J5.10, J6.9 y J6.10. 🔧 **29-sep: dibujados (F4…F7), pero con `C523825` (1,1 A) y no con `C883126` (500 mA). Decidir.**
      Los `.9` llevan `+3V3` **directo**.
- [x] **Pull-up de 10 kΩ en AMUTEO** (R24) — cuatro salidas open-drain en wired-OR contra PC10,
      hoy sin ningún pull-up externo: funciona sólo porque el firmware habilita el interno.
- [ ] **Fila de 8 pads a paso 2,54 mm para el SAI** (MCLK, SCK, FS y los cuatro SD).
      **US$ 0.** Sacar el stub del lado del STM32, no del DAC.
- [ ] **Los 4 LEDs de los jacks a GPIOs separados** (PE9, PE10, PE13, PE14). R111-R114 ya
      existen y van todos a PE12, que el firmware declara sin usar. Sin agregar componentes.
- [ ] **Partir el `~SD` de los amplificadores en cuatro bancos** (PD14, PD15, PD5, PD6).
      ≈US$ 0,01.
- [x] **Línea de timing del ESP32: NO usar PC13** — PL §3. ✓ 29-sep: GPIO4 ↔ R132 ↔ **PD2**. Choca con la errata **ES0392
      §2.2.30**: conmutar PC13 en LQFP corrompe el LSE y el workaround del documento es
      "ninguno". Elegir otro pin.
- [ ] **El tap de VOUT no cierra el lazo DAC→ADC** — las 8 entradas del AK5538 ya están
      tomadas por J9. Para el self-test usar **PC0**.
- [ ] **C45/C46 de 0201 a 0402** — PL §5.
- [ ] **Barrel jack DC-005, DNP**, en paralelo con la Phoenix, antes del TVS/buck.
      Serigrafiar **"19-20 V"**, la polaridad centro-positivo y **"USE ONE"** — PL §6b.
- [ ] **Series de MCLK: unificar en 33 Ω y ponerlas junto al STM32** (hoy son 75 Ω a los DAC
      y 33 Ω al ADC, elegidas por stock).

---

## C. Parlantes y arnés

- [ ] 📌 **La polaridad del conector de parlantes es `+ − − +`** — PL §0.8.1. Verificado en
      los 16 conectores. ⚠️ **El campo `pinfunction` del netlist miente sobre esto**: viene
      del símbolo y es igual en los 32 amplificadores. Lo que manda son los **nombres de
      red**, que se invierten entre A1 y A2.
- [ ] 🔴 **Ponerle nombre a las 64 redes de parlante: `SPK_CH00_P` / `SPK_CH00_N`…**
      Hoy son anónimas, y por eso el cruce de pares sobrevivió sin que nadie lo viera.
- [x] ~~JSPK09 y JSPK10 cruzados~~ — **ya resuelto en el diseño nuevo** (Fernando, 2026-09-11).
- [ ] **El arnés del 2×16 se alarga** — PL §0.8.1. Revisar largos antes de cortar.
- [ ] **Para cortar la madera**: `tools/geometria_array.py --layout 2x16 --dxf panel.dxf
      --diametro <el tuyo>`. Saca las 32 posiciones **de los `#define` del firmware**, así
      la madera y el beamforming no pueden discrepar. Avisa si los agujeros se superponen.

---

## D. Antes de exportar los gerbers

- [ ] **Importar el esquemático al PCB**: al 29-sep faltan C50, F4–F7, R24, R159–R161, y en el
      PCB R132 sigue entre GPIO8 y PC13. Después: borrar los 42 tramos de cobre sin red, cortar
      la pista vieja de J7.1 a `+3V3` y la de PC7 a `PDN_AK`, y mover D2 y L1, que crecieron
      sobre pistas ajenas.
- [ ] **DRC en 0** (hoy queda 1: el QR contra el texto `JLCJLCJLCJLC`, que se va solo
      cuando muevas ese texto).
- [ ] **`tools/check_pcb.sh`** en verde — o directamente commitear, que el hook lo corre.
      KiCad **no recalcula los rellenos al plotear**: usa el cobre guardado. Ya arruinó dos
      tandas.
- [ ] **`kicad-cli pcb export gerbers --check-zones`** — viene apagado por default.
- [ ] 🔴 **Al hacer el pedido: `Remove Order Number` → "Specify a location".** El marcador
      `JLCJLCJLCJLC` ya está bien puesto en la placa; si no elegís esa opción, nadie lo busca
      y **las 12 letras se imprimen literales** — que es lo que pasó en la DS11. AU §3 del
      bloque "Las tres decisiones del 11-sep".
- [ ] **Rotaciones de JLCPCB** — revisar contra la errata de la DS11 antes de mandar
      (JST = 180°, PRTR/SOT-143 = 90°, 220 µF = 180°, AP2125).
- [ ] **Stock**: no hace falta revisarlo — los componentes de esta tirada ya están comprados.

---

## E. Lo que hay que medir en el banco — AL §9

Cuando llegue la placa, y **antes** de darla por buena:

- [ ] Las tensiones de los tres rieles contra los valores de diseño: **5,088 V**, **3,280 V**,
      **3,3 V**.
- [ ] **FFT de `+5VA` con los amplificadores a volumen y los dos bucks corriendo.** Si
      aparece algo entre 20 Hz y 20 kHz que no esté con el buck B deshabilitado, el batido
      entre los dos conversores existe y hay que atacar el **layout** — AL §3.3.
- [ ] **El modo USB-solo con el ESP32 transmitiendo**, no en reposo: es el momento en que el
      pico de 900 mA es real.
- [ ] El UVLO: bajar la fuente y ver que **los amplificadores caen y el MCU sigue vivo
      reportando**.
