# DS10 Pre-Production Checklist

> **Fecha de revision**: 2026-04-03
> **Orden**: 5 unidades, JLCPCB
> **Branch**: main (tag DS10)
> **DRC**: 0 violations, 0 unconnected pads (2026-04-02)
> **ERC**: 11 errors (6 PWR_FLAG + 3 AMUTEO + 2 barrel jack switch), 21 warnings (cosmetic)

---

## 1. ESQUEMATICO — Verificacion por netlist (574 nets)

### Power Supply (DS_Power.kicad_sch)
- [x] MP2315S (IC2): VIN=IC2/2→+24V, SW=IC2/3→L1/1, GND=IC2/4→GND
- [x] Feedback T-type: R34/R32/R31 → C62 junction (net confirmed)
- [x] AAM network: IC2/1→R35/2, C65/1, R36/1
- [x] BST: R37/1→IC2/5, C68 connected
- [x] EN divider: R33/1→+24V, R33/2→IC2/6
- [x] Input caps en +24V: C56 + C58 (con GND)
- [x] L1 inductor: L1/1→SW, L1/2→+5VA
- [x] SMBJ24A TVS (D8): D8/1→+24V, D8/2→GND
- [x] FB2 separa +5VA→+5V: FB2/1→+5VA, FB2/2→+5V
- [x] FB3 separa +5VA→+5VA_DAC: FB3/1→+5VA_DAC, FB3/2→+5VA
- [x] AP1117 (U11): U11/3→+5V(in), U11/2→+3V3(out), U11/1→GND
- [x] AP2125K (U10): U10/1→+5V(in), U10/5→+3.3VA(out), U10/3→EN via R30
- [ ] Verificar valores de R/C en esquematico visual (no derivable del netlist)

### AK5538VN ADC (AK5538.kicad_sch)
- [x] AVDD1/AVDD2: IC3/2, IC3/15 → +3.3VA
- [x] TVDD: IC3/25 → +3V3
- [x] VDD18: IC3/27 → C268+C271 a GND (LDO interno, solo caps)
- [x] LDOE: IC3/57 → +3V3 (LDO habilitado)
- [x] VREFH1: IC3/62→R102→+3.3VA, C258+C257 (decoupling)
- [x] VREFH2: IC3/6→R103, C262+C261
- [x] VREFH3: IC3/11→R104, C263+C264
- [x] VREFH4: IC3/19→R105, C267+C270
- [x] VREFL1-4: IC3/61,5,12,20 → GND
- [x] TDM0: IC3/51 → GND (L)
- [x] TDM1: IC3/52 → +3V3 (H) → **TDM256 mode**
- [x] I2C enable: IC3/54 → +3V3
- [x] PW0-PW2: IC3/29,30,31 → +3V3 (32-bit word)
- [x] MSN: IC3/32 → GND (slave mode)
- [x] OVF: IC3/42 → U7/77 (PA15)
- [x] BICK: IC3/33 → U7/55 (SAI3_B SCK)
- [x] SDTO1: IC3/36 → U7/56 (SAI3_B SD)
- [x] LRCK: IC3/34 → U7/57 (SAI3_B FS)
- [x] MCLK: IC3/24 → SAI1_MCLK (12.288MHz compartido)
- [x] I2C3 SDA: IC3/43 → U7/66 + RN6/2 (pull-up)
- [x] I2C3 SCL: IC3/45 → U7/67 + RN6/1 (pull-up)
- [x] DIF1: IC3/50 → U7/65
- [x] PDN_AK: IC3/28 → U7/64
- [x] 16× 100pF en AINxP/AINxN: cada entrada tiene cap a GND (C274-C293 verified)
- [x] Pins GND: IC3/1,5,12,16,20,23,26,32,46,47,48,49,51,53,55,58,61,65 → GND
- [x] Pins no usados: IC3/35,37,38,39,40,41 → unconnected (marcados en netlist)

### DACs (DAC_PCM1690.kicad_sch)
- [x] SAI1_A (FS/SCK/SD): RN2/1,2,3 + RN3/1,2 → U7/3,4,5
- [x] SAI1_B (SD): RN3/3 → U7/2
- [x] SAI2_A (FS/SCK/SD): RN4/1,2 + RN1/1,2,3 → U7/59,60,58
- [x] SAI2_B (SD): RN4/3 → U7/41
- [x] I2C1: U1-U4/20(SDA)+/21(SCL) → RN5 → U7/96,95
- [x] RST: U1-U4/15 + R16/2 → U7/79
- [x] AMUTEI: U1-U4/18 + R15/2 → U7/80
- [x] AMUTEO: U1-U4/19 → U7/78 (wire-OR)
- [x] +5VA_DAC: U1-U4 pins 25,47 + FB3/1 + decoupling caps
- [x] MCLK: SAI1_MCLK → RN1/4, RN2/4, RN3/4, RN4/4, IC3/24

### Amplificadores (Amp_Bank.kicad_sch)
- [x] 32× PAM8302A (U12-U43): cada uno pin 3(A+), 4(A-), 5/8(OUT)
- [x] SD shutdown: 32 PAMs pin 1 → /ALL_AMPS/SD → R20 → U7/84 (PD3)
- [x] Ferrite beads: FB4-FB67 en todas las salidas OUT+/OUT-
- [x] JSPK01-JSPK16: todos presentes con 4 pines + FB + cap por canal

### STM32H743 (stm32_custom.kicad_sch)
- [x] VBAT: U7/6 → +3V3
- [x] VREF+: U7/20, U7/21 → FB1 + C35,C44,C43
- [x] VDD: U7/11,27,50,75,100 → +3V3
- [x] VSS: U7/10,19,26,49,74,99 → GND
- [x] BOOT0: U7/94 → K3 + C49 + R21/R14
- [x] NRST: U7/14 → K4 + C36,C52 + IC1(supervisor) + J7/6
- [x] SWD: SWDIO=U7/72→J7/2, SWCLK=U7/76→J7/3, SWO=U7/89→J7/5
- [x] HSE crystal: U7/12→Y2/3, U7/13→Y2/1 + C54,C53
- [x] LSE crystal: U7/8→Y1/2, U7/9→Y1/1 + C45,C46
- [x] UART ESP32: U7/22→U5/13(RX), U7/23→U5/12(TX)

### ESP32-C3-Mini (esp32-mini.kicad_sch)
- [x] GPIO8: U5/22 → R8/2(pull-up→+3V3) + K1/2 + C28
- [x] GPIO9: U5/23 → R106/1 (R106/2→+3V3) — **pull-up externo confirmado**
- [x] VCC: U5/3 → +3V3
- [x] GND: U5/1,2,11,14,36-53 → GND (todos los pads)
- [x] EN: U5/8 → R5/1 + C25 (RC reset)
- [x] USB: U5/26,27 → U6(ESD) → J3(USB-C) via R12,R13
- [ ] Antena: verificar keepout zone en PCB (no derivable del netlist)

### Conectividad
- [x] RJ45 ×4: J27,J28,J29,J30 con 8 pines cada uno → pares ADC AINxP/AINxN
- [x] Barrel jacks: J31/1→+24V, J31/2→GND, J32/1→+24V, J32/2→GND
- [x] J5 expansion: I2C2 (U7/46=SCL, U7/47=SDA + RN6/3,4) + SAI3_A + USART2
- [x] J6 expansion: I2S2 + SPDIFRX + USART2
- [x] JP2: SAI1_MCLK → J5/7 (MCLK jumper)
- [x] JP4: +3V3 → J7/1 (SWD VCC)
- [x] JP3: +5V → J7/10 (SWD power)

---

## 2. PCB LAYOUT — Verificacion visual en KiCad

### Stackup y capas
- [ ] 4 capas seleccionadas: JLC04161H-7628
- [ ] L1 (Top): componentes + señales
- [ ] L2 (In1.Cu): plano GND **solido, sin trazas, sin cortes**
- [ ] L3 (In2.Cu): zonas +5VA, +3V3, +3.3VA con GND fill
- [ ] L4 (Bottom): pasivos, PAM outputs, aux

### Zonas de potencia (L3)
- [ ] +5VA: zona grande, cubre area de amplificadores
- [ ] +3V3: zona media, centro (STM32/ESP32)
- [ ] +3.3VA: zona chica, bajo ADC
- [ ] GND fill en areas restantes de L3
- [ ] Clearances: +24V→GND 0.5mm, +5VA→GND 0.3mm, +3V3/+3.3VA→GND 0.25mm

### Thermal — MP2315S (IC2)
- [x] Solid copper fill en nodo SW / inductor L1 en L1
- [x] Via stitching GND alrededor de IC2 (vias L1→L2)
- [x] L2 intacto debajo del buck (sin cortes ni trazas)
- [ ] Input caps (C56, C58) < 3mm de IC2
- [ ] Output caps < 3mm de L1 inductor
- [ ] No hay zona +5VA ni +24V flood debajo del buck en L1

### Decoupling
- [ ] 100nF HF caps < 1mm de cada pin VDD de IC
- [ ] AP1117 output caps < 2mm de U11
- [ ] AP2125K output caps < 2mm de U10
- [ ] STM32: caps en todos los pares VDD/VSS
- [ ] AK5538: caps AVDD cerca de pins 2 y 15

### Señales criticas
- [ ] MCLK 12.288MHz: ruta lo mas corta posible, sin stubs
- [ ] SAI buses: clearance ≥0.2mm entre lineas del bus
- [ ] Pares diferenciales DAC→RJ45: length matched razonablemente
- [ ] I2C traces: no cruzan sobre zonas de switching del buck

### Vias
- [x] Ninguna via con drill < 0.3mm (**DRC: 0 violations**)
- [ ] Via stitching L3 GND → L2 cada 8-10mm
- [ ] Power vias +5VA/+3V3: 0.4/0.8mm en grupos de 2-3

### Solder mask y silkscreen
- [ ] Polaridad marcada en caps electroliticos, diodos, TVS
- [ ] Pin 1 marcado en STM32 (LQFP100), AK5538 (QFN64), PCM1690
- [ ] Texto de referencia legible (min 0.8mm height, 0.15mm width)
- [ ] JSPK01-16 visibles en silkscreen cerca de cada conector
- [ ] Marca "DS10" visible en silkscreen

---

## 3. DRC / ERC — Resultados (2026-04-02)

### DRC: PASSED
- [x] 0 DRC violations
- [x] 0 unconnected pads
- [x] 0 footprint errors
- [x] Vias < 0.3mm: resueltas
- [x] Unconnected pads: resueltas
- [x] Dangling tracks: eliminadas

### ERC: 11 errors, 21 warnings
**Errors (no funcionales):**
- [ ] 6× power_pin_not_driven: agregar PWR_FLAG en #PWR01, #PWR090, #PWR0115, VDDA, #PWR0178, #PWR0177
- [ ] 3× AMUTEO output-output (DAC0/DAC1/DAC2): suprimir (wire-OR intencional)
- [x] 2× J31/J32 Pin 3 unconnected: switch pin barrel jack — intencional, no implementado

**Warnings (cosmeticos):**
- [ ] 8× endpoint_off_grid (sheet root): wires 125/75mil off grid — limpiar alineacion
- [x] 2× bidirectional+power_output (DAC1/DAC3): ADR1 pin a +3V3 — correcto
- [x] 1× +3V3/VBAT multiple names: intencional (VBAT tied to +3V3)
- [x] 4× SAI FS/SCK shared labels: intencional (SAI1→DAC0+DAC1, SAI2→DAC2+DAC3)
- [x] 3× AMUTEO/RST/AMUTEI vs PCxx labels: intencional (global labels)
- [x] 2× +5V/VIN, +5VA/VDD: intencional (local labels)
- [x] 1× SAI1_MCLK/MCLK: intencional (AK5538 sheet local label)

---

## 4. ARCHIVOS DE FABRICACION

### Gerbers
- [ ] Generar Gerbers frescos desde KiCad (no usar archivos viejos)
- [ ] Verificar que incluye las 4 capas: F.Cu, In1.Cu, In2.Cu, B.Cu
- [ ] Verificar F.Mask, B.Mask, F.SilkS, B.SilkS
- [ ] Edge.Cuts presente y cerrado
- [ ] Drill file incluido (.drl)
- [ ] Abrir en visor Gerber externo (gerbview o JLCPCB viewer) y comparar con PCB

### BOM
- [ ] Cada componente tiene LCSC part number (campo "LCSC" = C-number)
- [ ] Cantidad en BOM matchea cantidad en Pick&Place
- [ ] STM32H743VIT6 (U7): marcado como consignment
- [ ] Componentes THT excluidos del BOM de JLCPCB (RJ45, barrel jacks, JSPK)
- [ ] Verificar que no hay componentes con LCSC "Extended" que esten out of stock

### Pick & Place
- [ ] Rotaciones corregidas (script Python o KiCad Fabrication Toolkit):
  - LQFP-100: -90°
  - SOP50P810X120-49N (PCM1690): -90°
  - SOT-223-3: 180°
  - SOT-23-3: 180°
- [ ] Fiducials presentes en P&P pero NO en BOM (correcto)
- [ ] Coordenadas de referencia: verificar que matchean el PCB

---

## 5. BOM — Verificacion de stock LCSC

| Componente | Ref | Qty/board | Verificar stock |
|---|---|---|---|
| STM32H743VIT6 | U7 | 1 | [ ] Consignment confirmado |
| AK5538VN | IC3 | 1 | [ ] Stock disponible |
| PCM1690 | U1-U4 | 4 | [ ] Stock disponible |
| PAM8302A | U12-U43 | 32 | [ ] Stock disponible |
| ESP32-C3-Mini | U5 | 1 | [ ] Stock disponible |
| MP2315S | IC2 | 1 | [ ] Stock disponible |
| AP1117-33 | U11 | 1 | [ ] Stock disponible |
| AP2125K-3.3 | U10 | 1 | [ ] Stock disponible |
| CBW201209U601T | FB4-FB67 | 64 | [ ] Stock disponible |
| SMBJ24A | D8 | 1 | [ ] Stock disponible |
| CAT24C256 | U8 | 1 | [ ] Stock disponible |
| MAX809 (supervisor) | IC1 | 1 | [ ] Stock disponible |
| USB ESD (U6) | U6 | 1 | [ ] Stock disponible |

---

## 6. OBSERVACIONES DEL ANALISIS DE NETLIST

### Cosas que estan bien y no necesitan accion
- [x] FB67 ahora es ferrite de salida U43 (Amp_Bank_A/AMP2_OUT-) — correcto, no es el viejo bridge
- [x] C290-C293: bulk caps adicionales en +5VA — bien agregados
- [x] R19 en paralelo con R20 en PD3 (/stm32_custom/PD3: U7/84, R20/1, R19/1) — verificar si es intencional (¿dos resistencias en paralelo al SD de los PAMs?)
- [x] USB-C shield: J3/S1 → R9/2 → GND (proteccion correcta)

### Puntos a revisar manualmente (no verificables por netlist)
- [ ] **R19 + R20 en paralelo a PD3**: ¿Es intencional tener dos resistencias en el SD de los PAMs? Si una es pull-down y otra es serie, OK. Si ambas son serie, el valor efectivo se reduce.
- [ ] **ESP32 GPIOs flotantes**: GPIO4,5,6,7,20,21 tienen nets pero sin destino (single-pin nets). No es critico pero puede aumentar consumo. Configurar como output-low en firmware.
- [ ] **STM32 GPIOs sin usar**: ~22 pines unconnected (PE0,PE1,PE9-15,PB2,PB4,PB15,PC0,PC1,PC6,PD2,PD5,PD6,PD14,PD15,PA5,PA9,PA10,PC13). Normal para LQFP100, configurar como analog en firmware para menor consumo.
- [ ] **8× endpoint_off_grid en sheet root**: Wires de 125/75mil off grid. No afecta funcion pero limpiar para evitar confusiones futuras.

---

## 7. PEDIDO JLCPCB — Parametros de configuracion

### PCB (pestaña "PCB")

| Parametro | Valor | Nota |
|---|---|---|
| Different Design | 1 | Un solo diseño |
| Delivery Format | Single PCB | No panel |
| PCB Qty | **5** | |
| PCB Thickness | **1.6mm** | Stackup JLC04161H-7628 |
| PCB Color | Green | Mas barato |
| Silkscreen | White | Default para green |
| Material Type | FR-4 | |
| Surface Finish | **HASL (with lead)** | Mas barato. ENIG si querés RoHS |
| Outer Copper Weight | **1 oz** | Stackup: 1oz/0.5oz/0.5oz/1oz |
| Via Covering | **Tented** | Cubre vias con solder mask (importante para vias termicas IC2) |
| Via Plating Method | Default | |
| Min via hole size | **0.3/0.6mm** | Tu minimo actual |
| Board Outline Tolerance | ±0.2mm | Default |
| Confirm Production file | **Yes** | Ingeniería revisa antes de fabricar |
| Mark on PCB | Order number (specify position) | Elegir lugar no critico en silkscreen |
| Electrical Test | **Flying Probe - Fully Test** | Obligatorio — 570 nets |
| Gold Fingers | No | |
| Castellated Holes | No | |
| Edge Plating | No | |
| Blind Slots | No | |
| UL Marking | No | |

### PCBA (pestaña "PCBA")

| Parametro | Valor | Nota |
|---|---|---|
| PCBA Type | **Economic** | Standard es para >20 unidades |
| Assembly Side | **Both Sides** | Componentes en Top y Bottom |
| PCBA Qty | **5** | |
| Tooling holes | **Added by JLCPCB** | Que ellos los ubiquen |
| Confirm Parts Placement | **Yes** | Revisar antes de soldar |
| Stencil Storage | No | |
| Fixture Storage | No | |
| Parts Selection | By JLCPCB | Via BOM con LCSC part numbers |

### PCBA Advanced Options

| Parametro | Valor | Nota |
|---|---|---|
| Bake Components | No | |
| Photo Confirmation | **Yes** | Ver soldadura STM32/AK5538 antes del envio |
| Board Cleaning | No | |
| Conformal Coating | No | Indoor, no necesario |
| Packaging | Default | Anti-static bag |
| Special stencil | No | |
| Depanel boards | No | |
| Solder Paste | Default | |
| Flying Probe Test | Yes | |
| Nitrogen reflow soldering | No | |
| Function test | No | JLCPCB no puede testear diseño custom |
| PCBA remark | `STM32H743VIT6 (U7) via consignment. Confirm orientation of LQFP100 (U7) and QFN-64 (IC3/AK5538) before soldering.` | |

### Componentes THT — Soldar a mano (NO incluir en BOM JLCPCB)

- RJ45 ×4: J27, J28, J29, J30
- Barrel jacks ×2: J31, J32
- Speaker connectors ×16: JSPK01–JSPK16
- C47 (1000µF electrolítico): soldar a mano

### Consignment (STM32H743VIT6)

1. Comprar en LCSC o Mouser
2. En JLCPCB → "Parts Manager" → transferir a SMT stock propio
3. Al subir BOM, marcar U7 como "consigned part"
4. JLCPCB confirma recepción antes de ensamblar

---

## 8. PWR_FLAG — Como agregar en KiCad

El ERC reclama `power_pin_not_driven` porque no puede detectar el driver de un net de potencia. Se resuelve agregando un símbolo `PWR_FLAG`:

1. Abrir el esquemático donde aparece el error (ej. DS_Power.kicad_sch)
2. Presionar **A** (Add Symbol)
3. Buscar `PWR_FLAG` en la librería `power`
4. Colocarlo tocando el net que reclama el error (ej. +5VA, +24V, VDDA)
5. No necesita nada más — solo que el pin del símbolo esté conectado al net

**Nets que necesitan PWR_FLAG** (del ERC actual):

- Sheet `/`: #PWR01
- Sheet `/DAC2/`: #PWR090
- Sheet `/esp32-mini/`: #PWR0115
- Sheet `/stm32_custom/`: VDDA
- Sheet `/DS_Power/`: #PWR0178, #PWR0177

---

## 9. DECISION FINAL

- [ ] Todos los items criticos de seccion 1 (esquematico) estan OK
- [ ] Seccion 2 (layout) verificada visualmente en KiCad
- [x] DRC: 0 errores
- [x] ERC: PWR_FLAGS y AMUTEO supresiones aplicadas (o aceptadas como-is)
- [ ] Stock LCSC confirmado para todos los componentes
- [ ] Consignment STM32 confirmado con JLCPCB
- [ ] Gerbers + BOM + P&P subidos a JLCPCB
- [ ] Confirm Parts Placement = Yes
- [ ] Photo Confirmation = Yes

### GO / NO-GO: _______________

> **Notas de la revision:**
>
> (espacio para anotar observaciones durante la revision)
>
>
> La verdad que no se como activar los power flags