# DS12 — Etapa de alimentación (especificación de diseño)

> Escrito el 2026-09-03. Resuelve el ítem **§0.1** de [DS12_PLANNING.md](DS12_PLANNING.md)
> y reabre el **§0.2**. Todo lo marcado ✅ está verificado contra `DS.txt` (netlist de
> producción de la DS11) o contra el datasheet citado; lo marcado ⚠️ hay que confirmarlo
> antes de congelar el diseño.
>
> **Esta es la placa definitiva.** El documento está escrito para poder dibujarse en KiCad
> sin volver a pensar nada: cada nodo, cada valor, cada orientación.

---

## 0. Qué cambia, en una tabla

| | DS11 (hoy) | DS12 (propuesto) |
|---|---|---|
| Entrada | Phoenix ×2 en el mismo net, **sin fusible ni protección de polaridad** | igual + **derivación local** con fusible y P-FET |
| TVS | SMBJ24A (VC 38,9 V) contra un buck de 26 V → **inútil** | **SMBJ20A** (VC 32,4 V) contra un buck de 35 V → **coordina** |
| Bulk de entrada | sólo cerámico (22 µF/50 V ≈ 9 µF reales) | + **100 µF/35 V electrolítico** (amortiguación del cable) |
| Buck | MP2315S, 3 A, 26 V abs, θJA 100 °C/W, **AAM/burst activo** | **AP63356DV-7**, 3,5 A, 35 V abs, θJA 25 °C/W, **PWM siempre**, con PG |
| Riel 3,3 V | `+5VA` → FB2 → D9 → AMS1117 (**margen de dropout ≈ 0**) | **AP63201WU-7**, buck propio desde la entrada |
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
             │  450 kHz    │  5,12 V / 3,5 A                      │  500 kHz    │  ◄── D20 (SS34)
             │  UVLO 15 V  │                                      │  AP63201WU-7│        │
             │  PG → STM32 │                                      │  3,30 V / 2A│  ◄── D21 (SS34)
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

**El AP63201 arranca desde 3,8 V.** Eso permite poner el OR **antes** del buck: el mismo
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
| **DZ1** | zener **12 V**, 500 mW | gate→source de Q1 (cátodo al source). **Obligatorio**: casi todos los P-FET chicos tienen VGS máx ±12 V o ±20 V, y la entrada es de 20 V |
| **D8'** | **SMBJ20A** (SMB) | `VIN_LOCAL` → GND, unidireccional |
| **C_bulk** | **100 µF / 35 V** electrolítico o polímero, ESR 0,2-0,5 Ω | `VIN_LOCAL` → GND |
| **C_in1** | 22 µF / 50 V X7R 1210 | `VIN_LOCAL` → GND |

**El orden importa: conector → fusible → P-FET → TVS → bulk.** Con el TVS *después* del
P-FET, la polaridad invertida simplemente apaga el FET y **no quema el fusible**; y como el
FET está saturado en operación normal, cualquier transitorio positivo llega igual al TVS.
El P-FET debe soportar el clamp: VDS ≥ 40 V contra los 32,4 V del SMBJ20A. ✅

> ⚠️ **El bulk electrolítico no es filtrado, es amortiguación.** La entrada actual es sólo
> cerámica: enchufar en caliente un ladrillo de 19-20 V por un cable largo es un LC serie
> sin amortiguar y **el pico llega a ~2× la tensión de entrada** (≈40 V), por encima del
> máximo absoluto de cualquier buck de esta clase. La regla es ESR ≈ √(L_cable/C_cerámico);
> con ~1 µH y ~10 µF eso da ≈0,3 Ω, que es justo la ESR de un electrolítico de 100 µF/35 V.
> **Este componente es lo que impide que el hot-plug mate la placa.**

### 3.2 Buck A — riel de amplificadores (`+5VA`, 5,12 V)

**AP63356DV-7** — Diodes Inc., V-DFN3020-13, LCSC **C2158014**.
3,8-32 V in, **35 V DC / 40 V por 400 ms** abs max ✅, 3,5 A, 450 kHz, **PWM siempre**,
Power-Good, θJA **25 °C/W** ✅ (datasheet DS41949 Rev 3).

| pin | red | componente |
|---|---|---|
| VIN | `VIN_LOCAL` | C 2× 10 µF/50 V X7R 1210 + 100 nF, **lo más cerca posible del pin** |
| EN | divisor de UVLO | R5 240 kΩ a `VIN_LOCAL`, R6 20 kΩ a GND → VON ≈ 15 V, VOFF ≈ 13 V ⚠️ |
| BST | a SW | C 100 nF / 16 V |
| SW | a L | **L 6,8 µH, Isat ≥ 5 A, DCR < 30 mΩ** |
| FB | divisor | R1 **162 kΩ** (de `+5VA`), R2 **30 kΩ** (a GND) → 0,8×(1+162/30) = **5,12 V** |
| COMP | — | según datasheet (compensación interna; ⚠️ confirmar si lleva RC externo) |
| PG | `PWR_GOOD_5V` | R 100 kΩ a `+3V3` → GPIO libre del STM32 |
| GND / pad | GND | vías térmicas bajo el pad |
| salida | `+5VA` | 2× 22 µF/16 V X7R + los 5× 220 µF que ya existen (C71, C290-293) |

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

### 3.3 Buck B — riel digital (`+3V3`, 3,30 V) ← **el integrado que faltaba**

**AP63201WU-7** — Diodes Inc., TSOT-26, LCSC **C2071044**, ~US$ 0,25, en la librería de
JLCPCB. (Datasheet DS41326 Rev 3-2, familia AP63200/01/03/05.)

| parámetro | valor | verificado |
|---|---|---|
| VIN recomendado | 3,8 – 32 V | ✅ |
| **VIN máximo absoluto** | **−0,3 a +35,0 V DC; −0,3 a +40,0 V por 400 ms** | ✅ |
| VEN máximo absoluto | 35 V | ✅ |
| Corriente | 2 A continuos; límite de pico HS 2,8 A típ | ✅ |
| fSW | **500 kHz** (AP63200/01) | ✅ |
| Modulación | **PWM a cualquier carga (AP63201)** | ✅ |
| tON mínimo | **80 ns** | ✅ |
| Referencia | 0,8 V ±1 % | ✅ |
| Protecciones | pico ciclo a ciclo, hiccup (2 ms on / 16 ms off), OVP, shutdown térmico 150/130 °C | ✅ |
| Soft-start | 4 ms interno | ✅ |
| θJA | 89 °C/W (TSOT26); TJ máx de diseño 125 °C | ✅ |

| pin | red | componente |
|---|---|---|
| 3 VIN | `VIN_B` | C 2× 10 µF/50 V X7R 1210 + 100 nF pegados al pin |
| 2 EN | `VIN_B` | **directo, sin divisor** — tiene que arrancar también con 4,3 V en modo USB |
| 6 BST | a SW | C 100 nF / 16 V |
| 5 SW | a L | **L 10 µH, Isat ≥ 3 A, DCR < 60 mΩ** (Sunlord MWSA0703S o equivalente; ya se usa MWSA0603S en L1) |
| 1 FB | divisor | **R1 75 kΩ** (de `+3V3`), **R2 24 kΩ** (a GND) → 0,8×(1+75/24) = **3,300 V**; **Cff 22 pF** en paralelo con R1 |
| 4 GND | GND | |
| salida | `+3V3` | 2× 22 µF/10 V X7R 0805 + 100 nF, más el desacople local que ya existe |

#### Por qué el '201 y no el AP63203 (que es 3,3 V fijo y parece más simple)

Las dos razones son numéricas, no de gusto:

1. **PFM.** El AP63203 soporta Pulse Frequency Modulation a carga liviana. Eso es
   exactamente el mismo mecanismo del AAM del MP2315S documentado en
   `presupuesto_potencia.md` §3bis C, que estamos arreglando en el buck A removiendo un
   componente. No tiene sentido sacarlo de un lado y meterlo del otro. El **AP63201 es el
   único de la familia que hace PWM a cualquier carga**.
2. **Ciclo de trabajo.** Con 20 V→3,3 V el duty es 16,5 %. A 500 kHz eso son
   **tON = 330 ns contra 80 ns de mínimo: 4× de margen**. El AP63203 corre a 1,1 MHz →
   tON = 150 ns a 20 V y **125 ns a 24 V**, al borde del salteo de pulsos, que es rizado de
   baja frecuencia inyectado en el riel de todo lo digital.

El precio de elegir el '201 es un divisor de dos resistencias. Es barato.

#### Un detalle no obvio: los dos bucks no deben tener la misma frecuencia

Dos conversores nominalmente iguales sobre el mismo riel de entrada **baten a la diferencia
de sus frecuencias reales**, que por tolerancia puede caer dentro de la banda audible.
AP63356 a 450 kHz + AP63201 a 500 kHz dan un batido en **50 kHz**, fuera de banda por
construcción. Es un argumento concreto para *no* poner dos veces el mismo integrado.

(El *frequency spread spectrum* de ±6 % que traen ambos no es un problema de audio: mueve
la portadora de 450/500 kHz, no crea energía nueva en banda base.)

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
`4,75 − 0,10 (F2) − 0,28 (D2 @0,5 A) − 0,28 (D21) = 4,09 V` → **por encima de los 3,8 V**
que pide el AP63201. Margen ≈ 290 mV. ⚠️ **Verificar con la fuente real que se vaya a
usar**; con un cargador decente (5,0-5,1 V) el margen sube a ~0,55 V.

### 3.5 Riel analógico `+3,3VA`

Sin cambios de topología: **U10 = AP2125K-3.3TRG1** (C110463) desde `+5V_AUX`. Lo que
cambia es el contexto: su entrada pasa de llevar 800 mA de corriente digital conmutada a
llevar ~60 mA, así que FB2 vuelve a comportarse como un filtro LC en vez de como una
resistencia saturada. ✅ La verificación pendiente del bring-up queda respondida por el
part number: **es un LDO fijo de 3,3 V, así que ese riel tiene que medir 3,3 V.**

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

| ref | descripción | LCSC | verificado |
|---|---|---|---|
| IC2 | **AP63356DV-7** buck 3,5 A (amps) | C2158014 | ✅ existe y está en JLCPCB |
| IC4 | **AP63201WU-7** buck 2 A (digital) | C2071044 | ✅ ~US$0,25, en JLCPCB |
| L1 | 6,8 µH, Isat ≥ 5 A, DCR < 30 mΩ | — | ⚠️ elegir |
| L2 | 10 µH, Isat ≥ 3 A, DCR < 60 mΩ | — | ⚠️ elegir |
| D8' | **SMBJ20A** TVS (reemplaza al SMBJ24A) | — | ✅ VRWM 20 V, VBR 22,2-24,5 V, VC 32,4 V |
| D2, D9, D11, D20, D21 | **SS34** SMA 40 V / 3 A (×5) | C8678 | ✅ ya en BOM |
| Q1 | P-FET, VDS ≥ −40 V, ID ≥ −3 A | — | ⚠️ elegir con VGS ≥ ±12 V |
| DZ1 | zener 12 V 500 mW | — | ⚠️ |
| F1 | fusible 2 A lento 1206 | — | ⚠️ |
| F2 | PTC 0,75 A hold / 1,5 A trip | — | ⚠️ |
| C_bulk | 100 µF / 35 V electrolítico o polímero | — | ⚠️ ESR 0,2-0,5 Ω, **no de baja ESR** |
| IC1 | **MAX809S** (reemplaza MAX809T) | — | ⚠️ |
| — | R 162 k, 30 k, 240 k, 20 k, 75 k, 24 k, 100 k ×2, 100 Ω, 10 k — todas 1 % | | |
| — | C 22 pF, 100 nF ×4, 10 µF/50 V ×4, 22 µF/16 V ×2, 22 µF/10 V ×2 | | |
| **U11** | **AMS1117-3.3 — SE ELIMINA** | | |

---

## 5. Modos de operación (tabla de verdad)

| 20 V | USB-C | ST-Link | `VIN_B` | `+3V3` | `+5VA` | `+5V_AUX` | comentario |
|---|---|---|---|---|---|---|---|
| ✅ | — | — | 19,7 V | 3,30 V | 5,12 V | 4,8 V | **normal. Es el caso que la DS11 nunca pudo hacer.** |
| ✅ | ✅ | — | 19,7 V | 3,30 V | 5,12 V | 4,8 V | idéntico; D2 en reverso, la PC no entrega nada |
| ✅ | ✅ | ✅ | 19,7 V | 3,30 V | 5,12 V | 4,8 V | idéntico; D11 en reverso |
| — | ✅ | — | 4,1-4,4 V | 3,30 V | **0 V** | 4,4-4,7 V | flasheo/bring-up sin fuente. Los amps callados (D9 bloquea) |
| — | — | ✅ (con USB) | 4,1-4,4 V | 3,30 V | 0 V | 4,4-4,7 V | igual, alimentado por la sonda |
| — | — | ✅ (sin USB) | 0 V | 0 V | 0 V | 0 V | **la sonda no alimenta nada y la placa no la alimenta a ella** |
| ✅ (cayendo <15 V) | — | — | ~14 V | 3,30 V | **0 V** | ~0 V | UVLO del buck A: **amps caen, MCU vivo y reportando** |

Los dos últimos renglones son los que hoy no existen y son la razón de todo el rediseño.

---

## 6. Análisis de fallas

| evento | qué pasa | quién protege |
|---|---|---|
| Polaridad invertida (jack barrel) | Q1 se apaga, nada conduce, **el fusible no se quema** | Q1 |
| Hot-plug de un ladrillo de 19 V | el LC del cable resuena; C_bulk lo amortigua; el pico residual lo clampea D8' a ≤32,4 V, bajo los 35 V DC de ambos bucks | C_bulk + D8' |
| Surge en la línea | D8' clampea a 32,4 V; F1 da la impedancia serie que el TVS necesita para coordinar | F1 + D8' |
| Corto en `+3V3` | AP63201: límite de pico ciclo a ciclo → hiccup 2 ms on / 16 ms off. El buck A sigue | IC4 |
| Corto en `+5VA` | AP63356: hiccup. **`+3V3` sobrevive** → el STM32 lo ve por PG y lo reporta | IC2 + PG |
| Corto en `+5V_AUX` o en la expansión | arrastra el AP2125 y la expansión, no el riel digital | (⚠️ considerar PTC en J5/J6 pin 9/10) |
| La placa alimentada quiere alimentar al VBUS del host | **imposible**: D2 y D11 están en reverso | D2, D11 |
| La placa alimentada quiere alimentar el 3V3 de la sonda | limitado a ~33 mA | R 100 Ω en J7.1 |
| La sonda quiere alimentar la placa por el pin 1 | limitado a ~33 mA: no arranca nada, y **no se apaga el LED del ST-Link** | R 100 Ω en J7.1 |
| Corto en la placa hacia el VBUS del USB | F2 abre | F2 |
| Sobretemperatura de un buck | shutdown a 150 °C, rearranque a 130 °C con soft-start | IC2/IC4 |
| Caída de la cadena por debajo de 15 V | amps OFF, MCU ON | UVLO del buck A |

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

- [ ] `+3V3` = **3,30 V ±0,07 V** con los 32 canales al máximo. (En la DS11: medir `+5V`
      con amps al máximo — si baja de 4,6 V, el §1 queda cerrado con número.)
- [ ] `+3,3VA` = **3,3 V** (cierra la verificación pendiente sobre U10).
- [ ] `+5VA` = **5,12 V ±0,1 V**.
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

- [ ] Fórmulas exactas de UVLO del AP63356 (DS41949, sección *Enable*): las de arriba
      están calculadas con las de la familia AP63200 (DS41326 Eq.1/Eq.2). Los valores
      240 k / 20 k son provisionales.
- [ ] Si el AP63356 lleva red RC externa en COMP o si la compensación es totalmente interna.
- [ ] Part numbers reales de L1, L2, Q1, DZ1, F1, F2, C_bulk y MAX809S, con stock en JLCPCB.
- [ ] Pinout del footprint V-DFN3020-13 (13 pads, no 8) — no reutilizar el del MP2315S.
- [ ] Si J5/J6 (que sacan `+3V3` y `+5V_AUX` al exterior **sin ninguna protección**,
      verificado en `DS.txt`) llevan PTC o no. Recomendado: sí, 500 mA en cada uno.
