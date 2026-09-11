# DS12 — Etapa de alimentación (especificación de diseño)

> Escrito el 2026-09-03. Resuelve el ítem **§0.1** de [DS12_PLANNING.md](DS12_PLANNING.md)
> y reabre el **§0.2**. Todo lo marcado ✅ está verificado contra `DS.txt` (netlist de
> producción de la DS11) o contra el datasheet citado; lo marcado ⚠️ hay que confirmarlo
> antes de congelar el diseño.
>
> **Esta es la placa definitiva.** El documento está escrito para poder dibujarse en KiCad
> sin volver a pensar nada: cada nodo, cada valor, cada orientación.
>
> ---
>
> ## 🔧 Revisión 2026-09-09 — part numbers verificados contra stock, y cuatro correcciones
>
> Se leyeron los datasheets completos (DS41326 Rev 3-2, DS41949 Rev 3-2, DS42002 Rev 3-2) y
> se verificó cada LCSC contra la biblioteca de JLCPCB. **Cambia el BOM.** Lo que estaba mal:
>
> 1. 🔴 **`C2158014` NO es el AP63356: es el AP63357**, que es la versión **PFM/PWM** — o sea
>    exactamente el modo de ahorro por ráfagas que estamos sacando del MP2315S. El AP63356DV-7
>    (PWM puro) es **`C2157973`**, y tiene **190 unidades** de stock.
> 2. 🔴 **El AP63201WU-7 (`C2071044`) tiene 210 unidades.** Se reemplaza por el
>    **AP63301WU-7 (`C2158003`)**: mismo TSOT-23-6, mismos 500 kHz, mismo PWM puro, 3 A en vez
>    de 2 A, **6650 unidades**, y **sin FSS** — que para audio es una ventaja, no una carencia.
> 3. 🔴 **El argumento del "batido en 50 kHz por construcción" del §3.3 es falso.** Los datasheets
>    especifican **rangos**, no valores: AP63356 = 400/450/500 kHz y AP63301 = 450/500/550 kHz.
>    **Se superponen entre 450 y 500 kHz.** Ver el §3.3 reescrito.
> 4. 🟡 **L2 son 6,8 µH, no 10 µH** (Tabla 1 de DS41326: 10 µH es el renglón de 5 V, no el de 3,3 V).
>
> ### 🔍 Revisión 2026-09-11 — auditoría adversarial
> Este documento pasó por una auditoría de 12 dimensiones ([DS12_AUDITORIA.md](DS12_AUDITORIA.md)).
> Se le encontraron **cinco errores**, ya corregidos en el texto y marcados 🔧: C56 no era
> cerámico, el §6 decía al revés lo de la expansión, la cita de la Tabla 1 para L2 estaba mal,
> el Cff de 220 pF quedaba 2,6× fuera de lugar, y el balance del modo USB usaba 0,5 A donde el
> propio documento dice 900 mA. Además se agregó el **§6.1**, con dos modos de falla que este
> rediseño crea y que no estaban previstos.
>
> Y se cerraron tres de los cinco pendientes del §10: **COMP va a GND**, las ecuaciones de UVLO
> son Eq.2/Eq.3 de DS41949, y **los dos divisores de realimentación salen con resistencias que
> ya están en la placa**. El BOM verificado, con stock, está en el **§11**.

---

## 0. Qué cambia, en una tabla

| | DS11 (hoy) | DS12 (propuesto) |
|---|---|---|
| Entrada | Phoenix ×2 en el mismo net, **sin fusible ni protección de polaridad** | igual + **derivación local** con fusible y P-FET |
| TVS | SMBJ24A (VC 38,9 V) contra un buck de 26 V → **inútil** | **SMBJ20A** (VC 32,4 V) contra un buck de 35 V → **coordina** |
| Bulk de entrada | **electrolítico** 22 µF/50 V (C56, `C72505`) + 100 nF — 🔧 *no era cerámico* | + **100 µF/35 V, ESR 600 mΩ** (más amortiguación, no la primera) |
| Buck | MP2315S, 3 A, 26 V abs, θJA 100 °C/W, **AAM/burst activo** | **AP63356DV-7**, 3,5 A, 35 V abs, θJA 25 °C/W, **PWM siempre**, con PG |
| Riel 3,3 V | `+5VA` → FB2 → D9 → AMS1117 (**margen de dropout ≈ 0**) | **AP63301WU-7**, buck propio desde la entrada |
| Riel 3,3 VA | AP2125 colgado del mismo `+5V` saturado | AP2125 sobre `+5V_AUX`, que ahora lleva 60 mA en vez de 800 |
| OR de fuentes USB | en la **salida** (`+5V`, con D2/D9/D11) | en la **entrada del buck B** (D20/D21) — el LDO desaparece |
| J7 pin 1 (3V3 ST-Link) | cable directo al riel | **100 Ω en serie** (VTref: sensa, no alimenta) |
| Supervisor | MAX809**T** (umbral hasta 3,23 V peor caso) | MAX809**S** (2,93 V) |
| Calor dentro de la caja | 1,2 W en el AMS1117 + ~1,5 W en el MP2315S | ~0,35 W + ~0,5 W |

---

## 1. Por qué la cadena actual no cierra ✅

Verificado en `DS.txt`. La cadena real es:

```
+5VA (5,09 V) ── FB2 (600Ω@100MHz, DCR 220 mΩ) ── D9 (SS34) ── +5V ── U11 ── +3V3
```

- El part number que está en el BOM **no es el AP1117 sino el AMS1117-3.3** (LCSC C6186):
  dropout **1,1 V típico, 1,3 V máximo** a 1 A.
- Con 0,6 A de riel digital: `5,09 − 0,35 (D9 @0,6 A) − 0,13 (FB2) = 4,61 V`.
- El AMS1117 necesita **4,4 V típico / 4,6 V peor caso** para mantener 3,3 V.

**El margen nominal es de 10 mV en el peor caso.** No es "poco margen": es una cadena que
no cierra por diseño, y explica exactamente el síntoma medido el 2026-09-02 (el STM32
reiniciándose cada ~10 s sin ST-Link, y siempre al encender los amplificadores).

El `+3,3VA` nunca fue el síntoma porque el AP2125 (**AP2125K-3.3TRG1**, C110463: 300 mA,
VIN máx 6 V, dropout **65 mV**, PSRR 65 dB ✅) tiene 20 veces más margen que el AMS1117.

**Corolario que hay que interiorizar: el problema nunca fue de energía almacenada.**
Repoblar C290 no lo iba a arreglar, y los 880 µF de bulk están del lado equivocado de
esos 0,48 V. Es un problema de **margen en serie**, y sólo se arregla sacando la serie.

---

## 2. La arquitectura nueva

```
 J11 ─┬───────────────────────────────────────────────────────────► J12 (sigue la cadena)
      │   el net +24V es un NODO DE PASO: nada en serie con él
      │
      └── DERIVACIÓN LOCAL ──────────────────────────────────────────────────────────────┐
          F1 (2 A) ── Q1 (P-FET, polaridad inversa) ── D8' SMBJ20A ── C_bulk 100 µF/35 V │
                                              │                                          │
                    ┌─────────────────────────┴──────────────────────────┐               │
                    │                                                    │               │
             ┌──────▼──────┐                                      ┌──────▼──────┐        │
             │  BUCK A     │  AP63356DV-7                         │  BUCK B     │        │
             │  450 kHz    │  5,088 V / 3,5 A                     │  500 kHz    │  ◄── D20 (SS34)
             │ UVLO 14,3 V │                                      │  AP63301WU-7│        │
             │  PG → STM32 │                                      │ 3,280 V / 3A│  ◄── D21 (SS34)
             └──────┬──────┘                                      └──────┬──────┘        │
                    │                                                    │               │
                  +5VA                                                 +3V3               │
                    ├── 32× PAM8302A + bulk 5×220 µF                     ├── STM32H743    │
                    ├── FB3 ── +5VA_DAC ── 4× PCM1690 VCC1/VCC2          ├── ESP32-C3     │
                    │                                                    ├── 4× PCM1690 VDD
                    └── FB2 ── D9 ──┐                                    ├── AK5538 TVDD  │
                                    │                                    ├── EEPROM       │
       USB-C (ESP32) VBUS ── F2 ── D2 ──┤                                └── pull-ups     │
       J7.10 (ST-Link 5 V) ──── D11 ──┤                                                   │
                                    │                                                     │
                              +5V_AUX ├── U10 AP2125 ── +3,3VA (AK5538 AVDD + 4× VREFH)   │
                                      ├── J5.10 / J6.10 (expansión)                       │
                                      └── D21 ───────────────────────────────────────────►┘

       J7.1 (3V3 del ST-Link) ── 100 Ω ── +3V3      (VTref: sensa, no alimenta)
```

### 2.1 La idea central: el OR se muda de la salida a la entrada

Hoy las tres fuentes (buck, USB-C, ST-Link) se suman **después** del regulador, sobre el
riel de 5 V, y de ahí sale un LDO. Esa topología es la que obliga a que el riel digital
cuelgue del riel de amplificadores.

**El AP63301 arranca desde 3,8 V.** Eso permite poner el OR **antes** del buck: el mismo
regulador sirve con 20 V (modo normal) y con los ~4,4 V que sobreviven del USB (modo
flasheo sin fuente). Consecuencias en cadena:

1. **U11 (AMS1117) desaparece del BOM.** Con él se van 1,2 W de calor y el único
   componente de la cadena que no cerraba.
2. **La protección del puerto USB no se toca**: siguen siendo D2 y D11, en el mismo lugar,
   con la misma orientación, haciendo lo mismo. Cambia sólo *qué alimentan*.
3. **FB2 + D9 se quedan**, pero ahora llevan ~60-250 mA en vez de 800: su caída pasa de
   **0,48 V a menos de 0,06 V**, y el AP2125 queda con 1,3 V de margen sobre su dropout
   de 65 mV. De paso el bead deja de estar saturado y **por primera vez filtra de verdad**
   el riel analógico del ADC.
4. En modo USB el rendimiento pasa de 66 % (LDO) a ~90 % (buck): la PC entrega
   **0,38 A en vez de 0,6 A**.

### 2.2 Por qué D20 y D21 no son opcionales

- **D20 (de `+24V` a VIN_B)**: sin él, los 5 V del USB suben al net `+24V`, salen por J11/J12
  y **aparecen en todas las placas de la cadena**, además de en la entrada del buck A.
  D20 conduce ~120 mA en modo normal (caída ≈ 0,25 V, pérdida 0,03 W).
- **D21 (de `+5V_AUX` a VIN_B)**: sin él, los ~19,7 V de VIN_B vuelven a `+5V_AUX`
  y **destruyen el AP2125**, cuyo máximo absoluto de entrada son 6 V. Reverso soportado
  en operación normal: ~15 V.

---

## 3. Esquemático, nodo por nodo

### 3.1 Entrada y protección

| ref | valor | conexión |
|---|---|---|
| **F1** | fusible SMD 1206, **2 A**, lento | `+24V` → `VIN_PROT` |
| **Q1** | P-MOSFET, **VDS ≥ −40 V**, ID ≥ −3 A, RDS(on) < 50 mΩ | cuerpo/diodo en el sentido de la corriente normal: **drain a `VIN_PROT`, source a `VIN_LOCAL`** |
| **R1** | 100 kΩ | gate de Q1 → GND (lo enciende: VGS ≈ −20 V) |
| **DZ1** | zener **10 V**, 500 mW (`C173431`) | gate→source de Q1 (cátodo al source). **Obligatorio**: el AO3401A tiene VGS máx **±12 V** y la entrada es de 20 V. 🔴 **10 V y no 12 V** — un zener de 12 V llega a 12,7 V por tolerancia y se pasa del límite del FET. Ver §11.2 |
| **D8'** | **SMBJ20A** (SMB) | `VIN_LOCAL` → GND, unidireccional |
| **C_bulk** | **100 µF / 35 V** electrolítico o polímero, ESR 0,2-0,5 Ω | `VIN_LOCAL` → GND |
| **C_in1** | 22 µF / 50 V X7R 1210 | `VIN_LOCAL` → GND |

**El orden importa: conector → fusible → P-FET → TVS → bulk.** Con el TVS *después* del
P-FET, la polaridad invertida simplemente apaga el FET y **no quema el fusible**; y como el
FET está saturado en operación normal, cualquier transitorio positivo llega igual al TVS.
El P-FET debe soportar el clamp: VDS ≥ 40 V contra los 32,4 V del SMBJ20A. ✅

> ⚠️ **El bulk electrolítico no es filtrado, es amortiguación.** Enchufar en caliente un
> ladrillo de 19-20 V por un cable largo es un LC serie, y sin amortiguar **el pico llega a
> ~2× la tensión de entrada** (≈40 V), por encima del máximo absoluto de cualquier buck de
> esta clase. La regla es ESR ≈ √(L_cable/C); con ~1 µH y ~10 µF eso da ≈0,3 Ω, del orden de
> la ESR de un electrolítico de 100 µF/35 V.
>
> 🔧 **CORRECCIÓN 2026-09-11.** La versión anterior decía *"la entrada actual es sólo
> cerámica"* y de ahí concluía que este componente *"es lo que impide que el hot-plug mate la
> placa"*. **Las dos cosas son falsas.** C56 es un **electrolítico de aluminio** de 22 µF/50 V
> (`C72505`, footprint `CP_Elec_6.3x5.4`, 43 mA de rizado @120 Hz): la DS11 **ya tiene**
> amortiguación en la entrada, y por eso cinco placas sobrevivieron el hot-plug. La derating
> por DC bias que se argumentaba (*"≈9 µF reales"*) **no aplica a un electrolítico**: 22 µF
> son 22 µF. El C_bulk de 100 µF sigue siendo la elección correcta —amortigua mejor y con
> ESR controlada— pero es una **mejora**, no lo que separa a la placa del desastre.
>
> Es el mismo patrón que ya quemó al proyecto cuatro veces: creerle al campo *value* en vez
> del footprint y el LCSC.

### 3.2 Buck A — riel de amplificadores (`+5VA`, 5,088 V)

**AP63356DV-7** — Diodes Inc., V-DFN3020-13, LCSC **C2157973** (⚠️ **no** C2158014, que es el
AP63357/PFM). 3,8-32 V in, **35 V DC / 40 V por 400 ms** abs max ✅, 3,5 A, **PWM siempre**,
Power-Good, θJA **25 °C/W** ✅, tON mínimo 100 ns típ / 120 ns máx, VFB 0,800 V ±1 %
(datasheet DS41949 Rev 3-2). **fSW = 400 / 450 / 500 kHz** (min/típ/máx — ver §3.3).

| pin | red | componente | ¿nuevo? |
|---|---|---|---|
| 1 VIN | `VIN_LOCAL` | **2× 10 µF/50 V X7R 1210** + 100 nF, pegados al pin | 1210 sí, 100 nF no |
| 2 EN | divisor de UVLO | **R3 470 kΩ** a `VIN_LOCAL` (`C23178`, = R33 de la DS11), **R4 40,2 kΩ** a GND (`C2933097`, = R34) | **no — las dos ya están** |
| 3 FB | divisor | **R1 40,2 kΩ** de `+5VA` (`C2933097`), **R2 7,5 kΩ** a GND (`C23234`) → **5,088 V** | **no — es el divisor de la DS11** |
| 4 COMP | **GND** | ✅ *"Connect the COMP pin to GND to use internal loop compensation"* — sin RC externo | — |
| 5 PG | `PWR_GOOD_5V` | R **100 kΩ** a `+3V3` (`C25741`) → GPIO libre del STM32 | no |
| 6 BST | a SW | C **100 nF** (`C1525`) | no |
| 7 NC | — | sin conexión interna; se puede dejar al aire | — |
| 8 GND / pad | GND | **vías térmicas bajo el pad** | — |
| 9 SW | a L1 | **L1 6,8 µH, Isat 8,5 A, 25 mΩ** — MWSA1004S-6R8MT (`C408485`) | sí |
| salida | `+5VA` | **Cff 220 pF** de `+5VA` a FB (`C39122`) + 2× 22 µF/16 V 0805 (`C45783`) + los 5× 220 µF que ya existen (C71, C290-293) | no |

**Los dos divisores salen gratis.** El de realimentación es **literalmente el de la DS11**:
R34 = 40,2 k y R32 = 7,5 k. Con el MP2315S (VFB = 791 mV) daban 5,031 V; con el AP63356
(VFB = 800 mV) dan `0,8 × (1 + 40,2/7,5) = **5,088 V**`. La impedancia es baja (107 µA de
corriente de divisor contra los 27 µA del ejemplo del datasheet), lo cual **es la dirección
segura**: el datasheet advierte contra valores *altos* por captación de ruido, no bajos.

**UVLO — ecuaciones Eq.2/Eq.3 de DS41949, verificadas.** El pin EN tiene un umbral de
1,18 V (subida) / 1,08 V (bajada) y dos fuentes de corriente: I1 = 1,5 µA siempre, I2 = 4 µA
extra cuando ya está encendido. Con R3 = 470 k y R4 = 40,2 k:

```
VON  = 1,18 × (1 + 470/40,2) − 1,5 µA × 470 k = 14,98 − 0,71 = 14,3 V
VOFF = 1,08 × (1 + 470/40,2) − 5,5 µA × 470 k = 13,71 − 2,59 = 11,1 V
```

> ⚠️ **Esto fija el sistema en 19-20 V y deja fuera el banco a 12 V.** Con VON = 14,3 V, una
> entrada de 12 V **nunca enciende los amplificadores** (el riel digital sí arranca: el buck B
> no lleva UVLO). Es coherente con el jack barrel serigrafiado "19-20V" y con la corrección de
> `presupuesto_potencia.md` §1 — y la razón por la que ahí se recomendaba 12 V (el límite de
> 1,25 W del MP2315S) desaparece con los 25 °C/W del AP63356. Si igual se quiere poder trabajar
> a 12 V, **cambiar sólo R4**, también con valores que ya existen:
>
> | R4 | VON | VOFF | para |
> |---|---|---|---|
> | **40,2 kΩ** (`C2933097`) | **14,3 V** | 11,1 V | **sistema 19-20 V — recomendado** |
> | 40,2 k + 5,1 k = 45,3 kΩ | 12,7 V | 9,7 V | 19-20 V con más tolerancia a caída de cadena |
> | 40,2 k + 20 k = 60,2 kΩ | 9,7 V | 6,9 V | permite banco a 12 V |

**La UVLO va acá y no en el buck B**, a propósito: con la cadena cayendo, **los
amplificadores se apagan primero y el MCU sigue vivo para reportarlo por red**. Es el
comportamiento que se quiere en una instalación de 15 nodos.

**Lo que este cambio resuelve de arrastre:**
- El ítem del **AAM/R36** desaparece estructuralmente: el AP63356 es PWM a cualquier carga,
  no hay burst en banda audible que remover con un componente.
- El límite de disipación de 1,25 W del MP2315S (`presupuesto_potencia.md` §1), que obligaba
  a elegir entre 12 V y 19-20 V, deja de ser el cuello de botella: 25 °C/W contra 100 °C/W.
- **PG** le da al STM32 un discriminador de brown-out del riel de amps **por hardware**,
  en vez de inferirlo del campo `reset` de `/status`.

### 3.3 Buck B — riel digital (`+3V3`, 3,280 V) ← **el integrado que faltaba**

**AP63301WU-7** — Diodes Inc., TSOT-26, LCSC **C2158003**, US$ 0,73, **6650 en stock**.
(Datasheet DS42002 Rev 3-2, familia AP63300/AP63301.)

> **Por qué el '301 y no el '201 que decía este documento.** El AP63201WU-7 (`C2071044`)
> es correcto en todo salvo en lo único que no se puede arreglar en el esquemático:
> **tiene 210 unidades en stock**. El AP63301 es el mismo integrado con más músculo —
> mismo encapsulado, mismo pinout, misma referencia de 0,8 V, mismos 500 kHz, mismo PWM
> puro — con 3 A en vez de 2 A y 30× más stock. El '201 queda como **segunda fuente
> pin-compatible**: si un día no hay '301, entra sin tocar el PCB.

| parámetro | AP63301 | verificado |
|---|---|---|
| VIN recomendado | 3,8 – 32 V | ✅ |
| **VIN máximo absoluto** | **−0,3 a +35,0 V DC; −0,3 a +40,0 V por 400 ms** | ✅ |
| VEN máximo absoluto | 35 V | ✅ |
| Corriente | **3 A** continuos | ✅ |
| fSW | **450 / 500 / 550 kHz** (min/típ/máx) | ✅ ver abajo |
| Modulación | **PWM a cualquier carga** | ✅ |
| **FSS** | **No** (el '300 sí lo tiene; el '301 no) | ✅ **ventaja para audio** |
| tON mínimo | **80 ns** | ✅ |
| Referencia | 0,8 V ±1 % | ✅ |
| Protecciones | pico ciclo a ciclo, hiccup, OVP, shutdown térmico | ✅ |
| Soft-start | 4 ms interno | ✅ |
| θJA | 89 °C/W (TSOT26) | ✅ |

| pin | red | componente | ¿nuevo? |
|---|---|---|---|
| 3 VIN | `VIN_B` | **2× 10 µF/50 V X7R 1210** + 100 nF pegados al pin | 1210 sí |
| 2 EN | `VIN_B` | **directo, sin divisor** — tiene que arrancar también con 4,1 V en modo USB | — |
| 6 BST | a SW | C **100 nF** (`C1525`) | no |
| 5 SW | a L2 | **L2 6,8 µH**, Isat 4,3 A, 31 mΩ — SWPA6045S6R8MT (`C57254`) | sí |
| 1 FB | divisor | **R1 62 kΩ** de `+3V3` (`C37825`), **R2 20 kΩ** a GND (`C25765`, = R31) → **3,280 V**; **Cff 100 pF** (`C327198`) en paralelo con R1 🔧 | **sólo el 62 k**: el 100 pF ya está en la placa |
| 4 GND | GND | | — |
| salida | `+3V3` | 2× 22 µF/16 V 0805 (`C45783`) + 100 nF, más el desacople local que ya existe | no |

🔧 **CORRECCIÓN 2026-09-11 — la cita del datasheet estaba mal.** La Tabla 1 de **DS42002**
(AP63300/AP63301) dice, para el renglón de **3,3 V**: R1 93,1 k · R2 30,1 k · **L 4,7 µH** ·
**C4 56 pF**. Los 6,8 µH son el renglón de **5,0 V**, no el de 3,3 V — el mismo error que la
revisión anterior creía estar corrigiendo, corrido un renglón. Y el *"196 k / 100 pF del
datasheet"* que se citaba **no existe en ninguna fila** de esa tabla.

**L2 = 6,8 µH se mantiene, pero como elección deliberada**: da ΔIL = 0,80 A a 19,7 V y 500 kHz,
menos rizado que los 4,7 µH de referencia. ⚠️ Con el inductor elegido (Isat 4,3 A) hay que
mirar el límite de pico del AP63301 — ver §11.2.

**El Cff pasa de 220 pF a 100 pF.** Con compensación interna fija, la única libertad del lazo es
dónde cae el par cero-polo del Cff, y 220 pF lo ponía **2,6× por debajo** del diseño de
referencia (cero 11,7 kHz / polo 47,9 kHz contra 30,5 / 125 kHz). Un polo en 47,9 kHz come
margen de fase cerca del cruce, y la carga que lo va a excitar es la peor posible: los escalones
de 100→350 mA del ESP32-C3 al transmitir, sobre el riel del que cuelgan el STM32 y el MAX809S.
Con 100 pF el cero queda en 25,7 kHz y el polo en 105 kHz, casi encima de la referencia.

**El divisor sigue costando un solo valor nuevo.** `0,8 × (1 + 62/20) = 3,280 V`, a −0,6 % de
3,300 — dentro del ±1 % de la propia referencia. R2 = 20 k **ya está en la placa** (`C25765`).

#### Por qué no el AP63203, que es 3,3 V fijo y parece más simple

Las dos razones son numéricas, no de gusto — y las dos están verificadas en DS41326:

1. **PFM.** El AP63203 hace *Pulse Frequency Modulation* a carga liviana. Es exactamente el
   mismo mecanismo del AAM del MP2315S documentado en `presupuesto_potencia.md` §3bis C, que
   estamos sacando del buck A. No tiene sentido removerlo de un lado y meterlo del otro.
   **El '201 y el '301 son los únicos de sus familias que hacen PWM a cualquier carga.**
2. **Ciclo de trabajo.** Con 20 V→3,3 V el duty es 16,5 %. A 500 kHz eso son **tON = 330 ns
   contra 80 ns de mínimo: 4× de margen**. El AP63203 corre a **1,1 MHz** (confirmado en la
   tabla de pedido) → tON = 150 ns a 20 V y **125 ns a 24 V**, cerca del salteo de pulsos, que
   es rizado de baja frecuencia inyectado en el riel de todo lo digital.

El precio de elegir el ajustable es un divisor de dos resistencias. Es barato.

#### 🔴 Los dos bucks y el batido — el argumento anterior estaba mal

La versión anterior de este documento decía que 450 kHz + 500 kHz dan *"un batido en 50 kHz,
fuera de banda por construcción"*. **Eso es falso, y la razón importa.** Los datasheets no
especifican una frecuencia, especifican un **rango**:

| | mínimo | típico | máximo |
|---|---|---|---|
| AP63356 (buck A) | **400 kHz** | 450 kHz | **500 kHz** |
| AP63301 (buck B) | **450 kHz** | 500 kHz | **550 kHz** |

**Los rangos se superponen entre 450 y 500 kHz.** Dos unidades concretas pueden salir de
fábrica a la misma frecuencia, y entonces la diferencia no es 50 kHz: es **cualquier cosa
entre 0 y 150 kHz**, derivando con la temperatura y barriendo la banda audible al pasar.
No hay ninguna elección de dos integrados de catálogo que garantice separación por
especificación, salvo irse a 1,1 MHz — que es justo lo que el punto 2 de arriba descarta.

**El mecanismo, para saber qué tan grave es.** La superposición lineal de dos tonos en el riel
de entrada no genera nada en banda base: dos tonos siguen siendo dos tonos. Lo que sí mezcla
es que **cada buck es un sistema muestreado a su propia fSW**: una perturbación del riel de
entrada a f₂ se pliega, dentro del conversor que conmuta a f₁, hacia **|f₁ − f₂|**. Ese
plegado sí cae en banda audible cuando las dos frecuencias están cerca.

**Por qué igual no es un problema en esta placa** — cuatro barreras en serie, todas medibles:

1. **Cada riel está regulado aparte.** El batido tiene que atravesar el lazo de realimentación
   del buck A, que por debajo de su cruce (~10-50 kHz) rechaza perturbaciones de línea con
   40-60 dB. La banda audible está muy por debajo de ese cruce: es donde **más** rechaza.
2. **El único camino audible es `+5VA`.** Lo que salga del buck B va a cargas digitales.
3. **Encima está el PSRR del PAM8302A**, 50 dB a 1 kHz.
4. **El FSS del AP63356 juega a favor, no en contra.** El buck A tiene *frequency spread
   spectrum* de ±6 % y **el AP63301 no tiene ninguno**. Esa asimetría es afortunada: la
   diferencia |f₁ − f₂| queda barrida sobre una banda de ~54 kHz en vez de quedarse quieta en
   una frecuencia. Un tono se convierte en ruido de banda ancha con densidad espectral dos
   órdenes más baja. **Es un argumento para elegir el '301 (sin FSS) y no el '300 (con FSS):**
   con FSS en los dos, los dos barridos podrían coincidir en fase y volver a concentrar energía.

**Lo que sí decide el resultado es el layout, no la elección de frecuencias**: el acoplamiento
entre los dos conversores es el que permita la impedancia común del riel de entrada. Ver §7.1 y
§7.2 — cada buck con su propio par de 10 µF pegado a su pin VIN, y el electrolítico de 100 µF
como nodo de baja impedancia compartido.

**Y se cierra midiendo, no discutiendo** (está en el §9): FFT de `+5VA` con los amplificadores
a volumen y los dos bucks corriendo. Si aparece algo entre 20 Hz y 20 kHz que no esté con el
buck B deshabilitado, el batido existe y hay que atacar el layout. Si no aparece, se terminó.

#### Presupuesto del riel de 3,3 V

| consumidor | típico | pico |
|---|---|---|
| STM32H743 @480 MHz VOS0 | ~300 mA | ~350 mA |
| ESP32-C3 con WiFi | ~100 mA | ~350 mA (TX) |
| 4× PCM1690 VDD | ~100 mA | ~120 mA |
| AK5538 TVDD + LDO interno | ~15 mA | |
| EEPROM, pull-ups, LEDs | ~30 mA | |
| **total** | **≈ 550 mA** | **≈ 900 mA** |

2 A es el tamaño correcto: 2× sobre el pico. Térmica a 0,6 A: Pd ≈ 0,33 W → +29 °C sobre
ambiente → con 40 °C dentro de la caja, TJ ≈ 69 °C. ✅ A 1,5 A sostenidos ya estaría en
TJ ≈ 118 °C, así que **el riel no debe crecer más allá de ~1 A** sin repensar el paquete.

Nota de arrastre: sacar esta carga de `+5VA` **le devuelve ~0,6 A al buck de los
amplificadores** (hoy el LDO consume 3 W del riel de 5 V para entregar 2 W).

### 3.4 El nodo `+5V_AUX` y el OR de fuentes

Renombrar el net `+5V` actual a `+5V_AUX` para que quede claro que ya no es el riel
principal. **Todos los diodos del OR pasan a SS34** (SMA, 40 V, 3 A, LCSC C8678) — un solo
part number, y de paso D2/D11 bajan su caída (el B5819W actual son 1 A):

| ref | de | a | función |
|---|---|---|---|
| D9 | `+5VA` (vía FB2) | `+5V_AUX` | impide que el USB intente alimentar los 32 amps |
| D2 | VBUS del USB-C (vía F2) | `+5V_AUX` | **impide backfeed al VBUS del host** |
| D11 | J7.10 (5 V del ST-Link) | `+5V_AUX` | **impide backfeed al VBUS del host vía la sonda** |
| D20 | `VIN_LOCAL` | `VIN_B` | impide que el USB suba a la cadena de 20 V |
| D21 | `+5V_AUX` | `VIN_B` | impide que los 20 V lleguen al AP2125 (abs max 6 V) |

**F2**: PTC repuesto **0,75 A hold / 1,5 A trip** en serie con el VBUS del USB-C. No es
redundante con el limitador del puerto de la PC: protege a la PC de *nuestra* placa, que es
la dirección que el host no controla. Caída ≈ 0,1 V.

Balance de tensión en modo USB solo (peor caso, VBUS 4,75 V al final de un cable):
`4,75 − 0,10 (F2) − 0,28 (D2 @0,5 A) − 0,28 (D21) = 4,09 V` → margen ≈ 290 mV.

🔧 **CORRECCIÓN 2026-09-11 — ese balance usa la corriente equivocada.** Los 0,5 A salen de la
nada: la tabla del **§3.3 de este mismo documento** dice **900 mA de pico** (ESP32 en TX +
STM32 + DACs). Y el modo USB-solo es justamente el de bring-up y flasheo, o sea el momento en
que ese pico es real. Rehecho con 0,9 A:

`4,75 − 0,15 (F2) − 0,36 (D2 @0,9 A) − 0,36 (D21) = 3,88 V` → **margen 70-130 mV**, no 290.

Y hay algo peor que el margen: **F2 tiene 750 mA de hold contra un pico de 900 mA**. Si
dispara, la placa se apaga y vuelve sola cada ~30 s — indistinguible de un brown-out, que es
exactamente el cuadro que costó semanas en agosto. Tres arreglos, todos baratos:
**(1)** F2 a **1,1 A de hold**; **(2)** D2 y D21 a un Schottky de menor Vf (SS54 o PMEG,
≈0,25 V a 0,9 A) → recupera ~200 mV; **(3)** medir el pico real antes de congelar.
La verificación del §9 tiene que ser **con el ESP32 transmitiendo**, no en reposo.

### 3.5 Riel analógico `+3,3VA`

Sin cambios de topología: **U10 = AP2125K-3.3TRG1** (C110463) desde `+5V_AUX`. Lo que
cambia es el contexto: su entrada pasa de llevar 800 mA de corriente digital conmutada a
llevar ~60 mA, así que FB2 vuelve a comportarse como un filtro LC en vez de como una
resistencia saturada. ✅ La verificación pendiente del bring-up queda respondida por el
part number: **es un LDO fijo de 3,3 V, así que ese riel tiene que medir 3,3 V.**

> Verificado 2026-09-09 contra la biblioteca de JLCPCB: `C110463` **es** el AP2125K-3.3TRG1,
> LDO SOT-23-5, 300 mA, dropout 65 mV @100 mA, PSRR 65 dB, 2316 en stock. El campo
> *description* del netlist de la DS11 describe un buck de 3 A en TSOT-23-8 — **está mal
> copiado, el part number es el correcto**. No hace falta cambiar nada; sí conviene arreglar
> el campo para que el próximo que lo lea no se confunda.

#### 🔴 ¿Se puede alimentar `+3V3` y `+3,3VA` desde una sola fuente, con un ferrite?

Es la pregunta obvia al ver que quedan dos reguladores para dos rieles de la misma tensión.
**La respuesta es no, y el motivo es un número, no una preferencia.**

Un ferrite no es un filtro de audio. El FB2 que hay en la placa (`C21286`) es
**600 Ω @ 100 MHz con 220 mΩ de DCR**. Esa es toda la historia: la impedancia de un ferrite
sólo existe cerca de su frecuencia nominal. **A 1 kHz un ferrite es una resistencia de
220 mΩ y nada más** — ofrece del orden de **0 dB de rechazo en toda la banda audible**.

| entre `+3V3` y `+3,3VA` | rechazo @ 1 kHz | rechazo @ 500 kHz |
|---|---|---|
| ferrite 600 Ω + 10 µF | **≈ 0 dB** | ~30-40 dB |
| **LDO AP2125** | **65 dB** | 40-50 dB |

Y hay una segunda razón, más terminante que la primera: **un LDO necesita caída de tensión
para regular**. De un riel de 3,3 V no se pueden sacar 3,3 V regulados — no hay headroom.
O sea que aunque se quisiera, el `+3,3VA` **no puede** derivarse del `+3V3`: el LDO tiene
que colgar de algo más alto, y eso es `+5V_AUX`. La topología no es una elección estética.

**Qué hay del otro lado de ese riel**: AVDD1/AVDD2 y **los cuatro VREFH del AK5538**. VREFH
es literalmente el fondo de escala del ADC — cualquier rizado ahí **es error de medición**,
no ruido de fondo. Es el peor lugar de la placa para ahorrarse 10 centavos de LDO.

**Lo que sí se unifica, y es el 90 % de la ganancia**: todo lo *digital* —STM32, ESP32,
los VDD de los cuatro PCM1690, el TVDD del AK5538, la EEPROM— pasa a colgar de **un solo
regulador** (el buck B), en vez de la cadena `+5VA → FB2 → D9 → AMS1117` que no cerraba.
De tres reguladores en serie se pasa a **dos en paralelo desde la entrada**. El ferrite
sigue existiendo, pero haciendo el trabajo para el que sirve: filtrar RF, con 60 mA
encima en vez de 800.

### 3.6 Supervisor: MAX809**S**, no MAX809T

El MAX809**T** tiene umbral **3,08 V típico con rango 2,92-3,23 V**. Un buck de 3,3 V ±2 %
puede entregar 3,234 V: por encima del típico pero **por debajo del peor caso del
supervisor**. Hoy zafa porque el LDO tira un poco alto. Con el riel bien regulado el margen
sigue siendo malo → **MAX809S (2,93 V típ)**. El STM32H743 funciona hasta 1,71 V y el
`VDD33USB` (ver [DS12_USB_STM32.md](DS12_USB_STM32.md)) pide ≥3,0 V, así que 2,93 V es el
umbral correcto: dispara antes de que el USB se caiga, y no antes de tiempo.

### 3.7 J7 (ST-Link): el pin 1 lleva resistencia, no diodo

`+3V3 ─ 100 Ω ─ J7.1`. Ver el ítem **2b** de [DS12_PLANNING.md](DS12_PLANNING.md) para el
razonamiento completo: ese pin es **VTref**, una entrada de sensado, no un riel. Un Schottky
resuelve el backfeed pero deja intacto el modo de falla que efectivamente ocurrió (la sonda
intentando alimentar 700 mA). D11 en el pin 10 se queda como está.

### 3.8 Fuga suelta que conviene arreglar de paso ✅

`PDN_AK` (pin PDN del AK5538 ← PC7 del STM32) **no tiene resistencia de pull-down**:
verificado en `DS.txt`, el net tiene sólo dos nodos. Después del reset el GPIO del STM32
queda en alta impedancia, así que el pin flota hasta que el firmware lo maneja. El
datasheet del AK5538 pide **PDN en "L" durante más de 150 ns después de que suben AVDD y
TVDD**. → **agregar 10 kΩ de PDN_AK a GND.**

---

## 4. BOM del bloque nuevo

> Sustituido por el **§11**, que tiene stock verificado. Se conserva sólo el saldo neto:
> **entran 9 part numbers nuevos, sale 1** (U11, el AMS1117-3.3), y las resistencias y
> capacitores del bloque salen **casi todos del BOM que la placa ya tiene**.

---

## 5. Modos de operación (tabla de verdad)

| 20 V | USB-C | ST-Link | `VIN_B` | `+3V3` | `+5VA` | `+5V_AUX` | comentario |
|---|---|---|---|---|---|---|---|
| ✅ | — | — | 19,7 V | 3,28 V | 5,09 V | 4,8 V | **normal. Es el caso que la DS11 nunca pudo hacer.** |
| ✅ | ✅ | — | 19,7 V | 3,28 V | 5,09 V | 4,8 V | idéntico; D2 en reverso, la PC no entrega nada |
| ✅ | ✅ | ✅ | 19,7 V | 3,28 V | 5,09 V | 4,8 V | idéntico; D11 en reverso |
| — | ✅ | — | 4,1-4,4 V | 3,28 V | **0 V** | 4,4-4,7 V | flasheo/bring-up sin fuente. Los amps callados (D9 bloquea) |
| — | — | ✅ (con USB) | 4,1-4,4 V | 3,28 V | 0 V | 4,4-4,7 V | igual, alimentado por la sonda |
| — | — | ✅ (sin USB) | 0 V | 0 V | 0 V | 0 V | **la sonda no alimenta nada y la placa no la alimenta a ella** |
| ✅ (cayendo <15 V) | — | — | ~14 V | 3,28 V | **0 V** | ~0 V | UVLO del buck A: **amps caen, MCU vivo y reportando** |

Los dos últimos renglones son los que hoy no existen y son la razón de todo el rediseño.

---

## 6. Análisis de fallas

| evento | qué pasa | quién protege |
|---|---|---|
| Polaridad invertida (jack barrel) | Q1 se apaga, nada conduce, **el fusible no se quema** | Q1 |
| Hot-plug de un ladrillo de 19 V | el LC del cable resuena; C_bulk lo amortigua; el pico residual lo clampea D8' a ≤32,4 V, bajo los 35 V DC de ambos bucks | C_bulk + D8' |
| Surge en la línea | D8' clampea a 32,4 V; F1 da la impedancia serie que el TVS necesita para coordinar | F1 + D8' |
| Corto en `+3V3` | AP63301: límite de pico ciclo a ciclo → hiccup 2 ms on / 16 ms off. El buck A sigue | IC4 |
| Corto en `+5VA` | AP63356: hiccup. **`+3V3` sobrevive** → el STM32 lo ve por PG y lo reporta | IC2 + PG |
| Corto en `+5V_AUX` | arrastra el AP2125 y la expansión, no el riel digital | F4/F6 |
| **Corto en la expansión (J5.9 / J6.9)** | 🔧 **arrastra `+3V3`**: STM32, ESP32, 4× PCM1690 y EEPROM. Esos pines llevan el riel digital **directo** | **F3/F5 — sin ellos no hay nada** |
| La placa alimentada quiere alimentar al VBUS del host | **imposible**: D2 y D11 están en reverso | D2, D11 |
| La placa alimentada quiere alimentar el 3V3 de la sonda | limitado a ~33 mA | R 100 Ω en J7.1 |
| La sonda quiere alimentar la placa por el pin 1 | limitado a ~33 mA: no arranca nada, y **no se apaga el LED del ST-Link** | R 100 Ω en J7.1 |
| Corto en la placa hacia el VBUS del USB | F2 abre | F2 |
| Sobretemperatura de un buck | shutdown a 150 °C, rearranque a 130 °C con soft-start | IC2/IC4 |
| Caída de la cadena por debajo de 15 V | amps OFF, MCU ON | UVLO del buck A |

---

## 6.1 🔴 Dos modos de falla que **crea** este rediseño (2026-09-11)

El UVLO del buck A es deliberado: *"los amplificadores se apagan primero y el MCU sigue vivo
para reportarlo"*. Pero eso **inventa un estado que la DS11 nunca tuvo** — el dominio analógico
hace un ciclo completo de apagado y encendido mientras el digital **nunca se cae**. Dos cosas
dependían de que todo se cayera junto:

### 6.1.a Los 32 pines SD quedan a 3,3 V con `+5VA` en 0 V

`/ALL_AMPS/SD` une los pines de shutdown de los 32 PAM8302A a un GPIO del STM32. Con el buck A
en UVLO y el B vivo, esos pines quedan **por encima del VDD del propio amplificador**, lo que
viola el máximo absoluto del PAM8302A. En la DS11 no pasaba porque `+3V3` colgaba de `+5VA` y
ambos morían juntos.

**Arreglo:** el firmware debe poner `SD` en bajo al ver caer `PWR_GOOD_5V` — pero eso es una
carrera contra la caída del riel. La versión por hardware es la misma que la de abajo.

### 6.1.b El AK5538 no vuelve a resetearse nunca

Peor porque es silencioso. Al volver la tensión, el AK5538 recupera AVDD **pero PC7 sigue en
alto desde antes** (el firmware lo puso ahí en el boot y no hubo reset del STM32). El chip
nunca recibe su pulso de ≥150 ns en bajo, así que su secuencia interna de arranque no vuelve a
correr y **queda en estado indefinido**: puede seguir entregando muestras que parecen válidas.

**Arreglo para los dos, dos componentes:** usar `PWR_GOOD_5V` también en hardware, no sólo como
GPIO de lectura →

```
PDN_AK = PC7  AND  PWR_GOOD_5V
```

Concretamente: PC7 maneja `PDN_AK` **a través de una R de 10 kΩ**, y de `PDN_AK` cuelga un
Schottky chico (BAT54) con el **cátodo a `PDN_AK` y el ánodo a `PWR_GOOD_5V`** (open-drain, que
se va a 0 V cuando el riel cae). Cuando PG baja, arrastra `PDN_AK`; cuando sube, la resistencia
deja que PC7 lo maneje normalmente. ≈US$ 0,03. **Se replica igual sobre la línea `~RST` de los
PCM1690**, que tienen exactamente el mismo problema: VCC1/VCC2 vienen de `+5VA`.

> Este es el costo oculto de separar los rieles, y es barato — pero hay que dibujarlo. Si se
> omite, el síntoma va a ser "a veces el ADC devuelve basura después de un bajón de tensión",
> que es de las cosas más caras de diagnosticar.

---

## 7. Reglas de layout (las que deciden si funciona)

1. **El lazo caliente de cada buck** — `Cin → VIN → SW → L` y el retorno por GND — es el
   único lazo que hay que minimizar a toda costa. Los 10 µF de entrada van **pegados** al
   pin VIN, con su retorno a GND por vías inmediatas. Un lazo grande acá es EMI radiada
   que después aparece en 64 salidas de parlante.
2. **La corriente RMS de entrada del buck A es 1,53 A** (Iout·√(D(1−D)) con D=0,256). Eso no
   lo aguanta un cerámico solo: dos 10 µF/50 V 1210 mínimo, ideal tres.
3. **La realimentación (FB) se rutea lejos del nodo SW**, tomando la muestra en el punto de
   carga, no en el pin del inductor. Traza fina, con guarda de GND si se puede.
4. **Cada buck con su propio "island" de GND de potencia**, unidos al plano principal en un
   solo punto cerca del bulk de entrada. **El plano de GND general NO se divide** (regla ya
   establecida en `PowerSpecs.md` y correcta).
5. **Vías térmicas bajo el pad del AP63356** (V-DFN3020): sin ellas los 25 °C/W del
   datasheet no existen — ese número es con 4 capas y 2 oz.
6. Los dos bucks **físicamente separados** del AK5538 y de los filtros de salida.
7. `+5V_AUX` puede ser una traza fina (≤250 mA); `+3V3` y `+5VA` son planos/polígonos.
8. **Punto de prueba en cada riel**, y esta vez que exista eléctricamente (ver §0.6 de
   DS12_PLANNING: seis de siete TP de la DS11 no están conectados).

---

## 8. ¿Reemplazar un LDO por un buck degrada el audio?

Es la objeción obvia y la respuesta es no, por dónde cae cada riel:

| riel | lo alimenta | qué hay aguas abajo |
|---|---|---|
| `+3V3` | **buck** (nuevo) | STM32, ESP32, **VDD digital** de los PCM1690, **TVDD** del AK5538, EEPROM |
| `+5VA_DAC` | buck A vía FB3 | **VCC1/VCC2 analógicos** de los PCM1690 |
| `+3,3VA` | **LDO** AP2125 | **AVDD1/AVDD2 y los 4 VREFH** del AK5538 |

**Ningún dominio analógico queda alimentado por el buck nuevo.** Lo que se le cuelga es
digital, y el rizado calculado es `ΔIL/(8·f·C) = 0,6/(8·500k·30µ) ≈ 5 mV` a 500 kHz — dos
décadas por encima de la banda de audio, sobre pines que ya tienen desacople local.

Y el riel que sí es analógico **mejora**: el AP2125 conserva sus 65 dB de PSRR y ahora su
entrada viene por un bead que ya no está saturado.

---

## 9. Verificación en banco (con los números que se esperan)

Antes de mandar a fabricar, sobre una DS11 modificada a mano o sobre el primer prototipo:

- [ ] `+3V3` = **3,28 V ±0,07 V** con los 32 canales al máximo. (En la DS11: medir `+5V`
      con amps al máximo — si baja de 4,6 V, el §1 queda cerrado con número.)
- [ ] `+3,3VA` = **3,3 V** (cierra la verificación pendiente sobre U10).
- [ ] `+5VA` = **5,09 V ±0,1 V**.
- [ ] **FFT de `+5VA` con los dos bucks corriendo y los amplificadores a volumen**, punta en
      AC, 20 MHz de ancho de banda: nada entre 20 Hz y 20 kHz que no esté también con el
      buck B deshabilitado. Es la prueba que cierra la discusión del batido (§3.3).
- [ ] Consumo real del riel de 3,3 V con shunt, contra los **550 mA** de la tabla.
- [ ] Rizado de `+3V3` con punta en modo AC, ancho de banda 20 MHz, resorte de masa:
      **< 20 mVpp**, y el espectro con el pico en 500 kHz, **nada en banda audible**.
- [ ] Temperatura de los dos bucks tras 30 min con el channel walker (peor caso térmico:
      seno sostenido en fase, crest factor 0 dB): **< 85 °C** de encapsulado.
- [ ] Los siete renglones de la tabla de modos del §5, uno por uno, con multímetro.
- [ ] Hot-plug 20 veces con el ladrillo real y punta en `VIN_LOCAL`: **el pico no supera
      los 32 V**.
- [ ] Polaridad invertida a propósito, con fuente en modo corriente limitada: consumo 0,
      el fusible entero.
- [ ] Con la placa alimentada y el ST-Link conectado pero **sin USB**: la corriente que
      entra por J7.1 debe ser **< 35 mA**, y el LED de la sonda no se apaga.

## 10. Lo que falta confirmar antes de congelar ⚠️

- [x] ~~Fórmulas exactas de UVLO del AP63356~~ → **Eq.2/Eq.3 de DS41949 Rev 3-2**, leídas de
      la Figura 37. Coinciden con la derivación por balance de corrientes en el nodo EN
      (I1 = 1,5 µA siempre, I2 = 4 µA extra al encender), y el resultado es
      **R3 = 470 k + R4 = 40,2 k → VON 14,3 V / VOFF 11,1 V, ambas ya en la placa**. Ver §3.2.
- [x] ~~Si el AP63356 lleva red RC externa en COMP~~ → **COMP a GND**. Tabla de pines de
      DS41949: *"Connect the COMP pin to GND to use internal loop compensation."*
- [x] ~~Part numbers reales de L1, L2, Q1, DZ1, F1, F2, C_bulk y MAX809S~~ → **§11**, todos
      con stock verificado en la biblioteca de JLCPCB al 2026-09-09.
- [ ] **Pinout del footprint V-DFN3020-13** (13 pads, no 8) — no reutilizar el del MP2315S.
      Sigue pendiente: es trabajo de KiCad, no de selección.
- [ ] **PTC en J5/J6.** Recomendado: sí. Part number ya elegido en el §11
      (BSMD1206-050-30V, `C883126`, 500 mA hold / 1 A trip, 30 V). 🔧 **Cuatro, no dos**: J5 y J6
      tienen **dos** pines de alimentación cada uno (pin 9 = `+3V3`, pin 10 = `+5V_AUX`).
- [ ] ⚠️ **Decidir la tensión de sistema**: con R4 = 40,2 kΩ el UVLO deja fuera el banco a
      12 V. Ver la tabla del §3.2 — es un cambio de una sola resistencia, pero hay que
      decidirlo antes de serigrafiar el jack.
- [ ] ⚠️ **Errata heredada de la DS11 que conviene arreglar de paso**: `C8678` (SS34) es
      **SMA / DO-214AC**, pero en la DS11 está asignado al footprint `Diode_SMD:D_SMB`
      (DO-214AA, más grande). Las cinco placas se armaron igual, pero para los cinco SS34 de
      la DS12 hay que **corregir el footprint a SMA** o comprar un SS34 en SMB.

---

## 11. BOM verificado (JLCPCB, 2026-09-09)

Stock y precio leídos de la biblioteca de JLCPCB el 2026-09-09. **El stock se mueve: hay que
volver a chequear antes de pedir.** Los renglones marcados 🟡 son los que conviene mirar primero.

### 11.1 Integrados — lo que se cambia

| ref | DS11 | **DS12** | LCSC | stock | US$ |
|---|---|---|---|---|---|
| **IC2** | MP2315SGJ-P (`C3031493`) | **AP63356DV-7** buck 3,5 A, amps | **`C2157973`** | 🟡 **190** | 0,68 |
| **IC4** | *(no existía)* | **AP63301WU-7** buck 3 A, digital | **`C2158003`** | 6 650 | 0,73 |
| **U11** | AMS1117-3.3 (`C6186`) | ❌ **SE ELIMINA** | — | — | — |
| **U10** | AP2125K-3.3TRG1 (`C110463`) | ✅ **se queda igual** | `C110463` | 2 316 | 0,10 |
| **IC1** | MAX809**T**EUR+T (`C16711`) | **MAX809S** 2,93 V, SOT-23 | **`C347371`** | 289 967 | 0,04 |
| **D8** | SMBJ24A (`C908801`) | **SMBJ20A** DO-214AA | **`C364296`** | 194 439 | 0,06 |

**IC1 y D8 son reemplazo directo sobre el footprint que ya existe** (SOT-23-3 y `D_SMB`
respectivamente): cambia el part number en el BOM, no el dibujo.

> 🟡 **Las 190 unidades del AP63356DV-7 son el único renglón ajustado.** Alcanzan de sobra para
> un lote de 5-10 placas, pero no para escalar. Alternativas ya identificadas, por si se agota:
> **AP63356QZV-7** (`C3194571`, 2 546 en stock, US$ 0,61) es la versión calificada para
> automoción del mismo silicio — ⚠️ encapsulado **VDFN-13(2x3)** con datasheet propio (DS41948):
> **verificar que el footprint sea el mismo antes de sustituir.**

### 11.2 Componentes nuevos — discretos y pasivos de potencia

| ref | qué es | part number | LCSC | stock | US$ |
|---|---|---|---|---|---|
| **Q1** | P-MOSFET polaridad inversa, −30 V, 4,2 A, 47 mΩ, SOT-23 | AO3401A | **`C15127`** ⭐**BASIC** | 715 902 | 0,07 |
| **DZ1** | Zener **10 V** 500 mW, SOD-123 (protege VGS de Q1) | BZT52C10 | **`C173431`** | 128 841 | 0,014 |
| **F1** | Fusible **2 A / 63 V**, 1206, corte 50 A | JFC1206-1200FS | **`C136345`** | 14 516 | 0,06 |
| **F2** | PTC USB **750 mA hold / 1,5 A trip**, 8 V, 1206 | SMD1206B075TFT | **`C269116`** | 4 304 | 0,04 |
| **F3…F6** 🔧 | PTC expansión — **4**: J5.9/J5.10/J6.9/J6.10 | BSMD1206-050-30V | **`C883126`** | 46 724 | 0,06 |
| **L1** | 6,8 µH, **Isat 8,5 A**, Irms 12 A, 25 mΩ, 11×10 mm | MWSA1004S-6R8MT | **`C408485`** | 1 981 | 0,25 |
| **L2** | 6,8 µH, Isat 4,3 A, Irms 3 A, 31 mΩ, 6×6 mm | SWPA6045S6R8MT | **`C57254`** | 16 727 | 0,11 |
| **C_bulk** | **100 µF / 35 V** electrolítico, ESR 600 mΩ, D6,3×7,7 mm | RVE100UF35V67RV0072 | **`C2836437`** | 398 000 | 0,05 |
| **C_in ×4** | **10 µF / 50 V X7R 1210** (2 por buck) | CL32B106KBJNNNE | **`C138687`** | 28 914 | 0,29 |
| **R_fb** | **62 kΩ 0402 1 %** — el único valor de resistencia nuevo | 0402WGF6202TCE | **`C37825`** | 26 024 | 0,001 |

**Notas de selección, con el número que las justifica:**

- **Q1 = AO3401A** y no un −40 V: en operación normal el FET está saturado (VDS ≈ 0) y el TVS
  está **aguas abajo**, así que el FET nunca ve los 32,4 V del clamp. Lo único que ve en
  inversión es la tensión de entrada, 20 V contra los −30 V del part. Y es **Basic de JLCPCB**:
  sin cargo de setup. Disipa 1,1 A² × 47 mΩ = **57 mW**.
- 🔴 **DZ1 es de 10 V, no de 12 V como decía el §3.1.** El AO3401A tiene **VGS máx ±12 V**, y un
  zener de 12 V tiene tolerancia hasta **12,7 V** — o sea que el componente que está puesto para
  proteger la compuerta la destruiría. Con 10 V (9,4-10,6 V) queda margen, y el RDS(on) de
  47 mΩ está especificado justamente a VGS = −10 V. **Si se prefiere un zener de 12 V, hay que
  cambiar Q1 por uno de VGS ±20 V** (p. ej. DMP4015SSS, `C5197355`, −40 V / 12 A / 18 mΩ,
  SOP-8, 4 067 en stock) — más robusto, pero más grande y sin ser Basic.
- **F1 es fusible de un solo uso, no PTC.** No hay ningún PTC de 1206 con ≥ 2 A de *hold* y
  ≥ 30 V de tensión: los de 30 V llegan a 1,1 A (demasiado cerca de los ~1,2 A de consumo real)
  y los de 2 A son de 12 V (por debajo de la entrada). Además el fusible da la **impedancia
  serie que el TVS necesita para coordinar** (§6). Los 63 V están holgados sobre los 32,4 V
  del clamp.
- **L1 tiene Isat 8,5 A** contra un límite de corriente del AP63356 de ~5 A: **el inductor no
  satura ni durante una falla**. Los 11×10 mm son el precio. Si el área aprieta, el mismo
  SWPA6045S6R8MT de L2 (4,3 A de Isat) sirve **si se acepta que sature en sobrecorriente** —
  no en operación normal, donde el pico es 3,6 A.
- **C_in tiene que ser 1210/50 V.** El único 10 µF que hay hoy en la placa (`C1691`) es
  **6,3 V en 0603**: sobre 20 V no es que filtre poco, es que **se destruye**. La corriente
  RMS de entrada del buck A es 1,53 A (§7.2) y por eso van **dos** por buck.
- **C_bulk: ESR 600 mΩ es la característica que se está comprando**, no un defecto. Es el
  amortiguador del LC del cable en el hot-plug (§3.1). **No sustituir por uno de baja ESR**,
  ni por un polímero, ni por cerámicos: sin pérdidas no hay amortiguación. El footprint
  D6,3×7,7 mm **ya existe en la placa** (es el de los 220 µF C71/C290-293).

### 11.3 Resistencias y capacitores — 10 de 11 salen del BOM que ya existe

Todo el bloque de alimentación se dibuja con valores que **ya están comprados y colocados** en
la DS11. El único agregado es el 62 kΩ:

| valor | LCSC | footprint | dónde se usa en DS12 | ya estaba en |
|---|---|---|---|---|
| **470 kΩ** | `C23178` | 0603 | R3, UVLO superior del buck A | R33 |
| **40,2 kΩ** | `C2933097` | 0402 | R4 (UVLO inferior) **y** R1 del divisor FB del buck A | R34 |
| **7,5 kΩ** | `C23234` | 0603 | R2 del divisor FB del buck A → 5,088 V | R32 |
| **20 kΩ** | `C25765` | 0402 | R2 del divisor FB del buck B → 3,280 V | R31 |
| **100 kΩ** | `C25741` | 0402 | pull-up de PG a `+3V3` | R30, R35 |
| **100 Ω** | `C177347` | 0603 | serie en J7.1 (VTref del ST-Link, §3.7) | R28 |
| **10 kΩ** | `C25744` | 0402 | pull-down de `PDN_AK` (§3.8) | R8, R14-R17… |
| **220 pF** | `C39122` | 0402 | Cff del divisor del **buck A** (§3.2) | C90, C91… (32 unidades) |
| **100 pF** 🔧 | `C327198` | 0402 | Cff del divisor del **buck B** (§3.3) — verificado en `BOM-DS.csv` el 2026-09-11: existe, 16 unidades | C274-C289 |
| **100 nF** | `C1525` | 0402 | 2× BST + bypass de VIN | 30 unidades |
| **22 µF / 16 V** | `C45783` | 0805 | 2× salida buck A + 2× salida buck B | C67, C69, C70, C273 |
| 🆕 **62 kΩ** | **`C37825`** | 0402 | R1 del divisor FB del buck B | **nuevo** |

Y siguen sirviendo tal cual: los **5× 220 µF/16 V** (`C25503905`) de bulk en `+5VA`, los tres
ferrites FB1/FB2/FB3 (`C21286`) y los **5× SS34** (`C8678`) del OR de fuentes — estos últimos
con la corrección de footprint de la errata del §10.

### 11.4 Saldo

| | |
|---|---|
| Part numbers que **entran** | 10 (2 bucks + FET + zener + 2 fusibles + PTC ×2 tipos + 2 inductores + electrolítico + 1210 + 62 k) |
| Part numbers que **salen** | 1 (AMS1117-3.3) |
| Part numbers que **cambian** sobre footprint existente | 2 (MAX809T→S, SMBJ24A→20A) |
| Valores nuevos de R o C | **1** (62 kΩ 0402) |
| Costo del bloque nuevo | **≈ US$ 3,00** por placa |
| Calor que se va de la caja | **≈ 1,85 W** (1,2 W del AMS1117 + ~0,65 W del MP2315S) |
