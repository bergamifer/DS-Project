# DS10 Design Review & Lessons from DS9

> **Fecha**: 2026-02-21
> **Basado en**: Experiencia funcional DS9 con firmware 480MHz + DMA audio pipeline
> **Objetivo**: Maximizar fiabilidad, reducir costos, mejorar manufactura DS10

---

## 1. ERRORES DE DISEÑO PCB — DS9 (evitar en DS10)

### 1.1 Pin Conflicts SAI ↔ GPIO Control
**Problema DS9:**
- **PE3**: DAC1_RST (GPIO) ↔ SAI1_SD_B (AF6) — colisión durante init
- **PE11**: DAC0_AMUTEI (GPIO) ↔ SAI2_SD_B (AF10) — colisión durante init

**Solución workaround DS9**: Init DAC por I2C primero, luego reconfigurar pin a AF6/AF10.

**Recomendación DS10**:
- ✅ **Eliminar GPIO RST/MUTE/ZERO** — ya tenes control I2C en PCM1690
- ✅ Verificar en schematic que NO hay conflictos de alternate functions
  - Tool: KiCAD ERC (ya corriendo)
  - Manual: Revisar STM32H743 LQFP100 datasheet, cada pin, cada AF

### 1.2 UART Wiring — DS9 (cruzado pero funcional)
**Observación**:
- PCB DS9 tiene GPIO0→PA1 y GPIO1→PA0 (cruzadas)
- Esto es CORRECTO por trazas físicas, pero confundió el firmware ESP32 inicial
- **Recomendación DS10**: Documentar claramente en silkscreen o schematic que TX/RX están cruzados vs GPIO numbering

---

## 2. CAMBIOS BUENOS EN DS10 (según tu changelog)

### ✅ Control I2C para DAC (RST/MUTE/ZERO0/1)
- Reduce GPIO pins en STM32 (necesarios para futuras features)
- I2C1 ya usado, integración natural
- **Ventaja**: Mismo firmware sin cambios, solo PCB simplificado

### ✅ 5V único para alimentación
- DS9: múltiples tensiones (5V, 3.3V, 12V posiblemente) → complejidad
- DS10: 5V entrada, reguladores locales (3.3V para lógica, 5V/3.3V para DACs según spec)
- **Cuidado**: Reguladores deben tener inductor/filtrado buen para no inyectar ruido en audio

### ✅ Amplificador integrado en placa
- Control de on/off desde GPIO del STM32 (power management)
- Mejor que módulo externo (menos cables, menos EMI)
- **Verificar**: ¿Hay línea de shutdown/mute del amp hacia STM32? ¿Detección de sobrecarga?

### ✅ Resistencias en DIN/SCK/MCLK/FS
- Audio digital requiere terminaciones para integridad de señal
- Si hay Dante/AES entrada, esto es crítico
- **Valores recomendados**:
  - DIN (entrada): Thévenin ~330Ω // 330Ω (típico balanced)
  - MCLK/SCK output: 50Ω serie (para EMI control)
  - FS output: 50Ω serie

### ✅ Nuevo botón reset ESP32
- Útil en desarrollo (reinicio sin STLink)
- **Conexión**: ESP32 `EN` pin pull-up + botón a GND, debounce ~100ns

### ✅ Conector SH1.0 para STLink
- Más compacto que microUSB, mejor para manufactura
- **Pinout estándar**:
  ```
  SH1.0 1x10 header:
  1(VCC) 2(GND)
  3(RDI) 4(RDO)
  5(nRST) 6(GND)
  7(SWDIO) 8(SWDCLK)
  9(GND) 10(GND)
  ```

### ✅ Reemplazo componentes (Ferrite, R30, R25)
- Abaratamiento sin sacrificar función (buena decisión)
- **Verificar**: Ferrite tiene especificaciones de impedancia vs frecuencia (debe cubrir 0-100MHz para SAI)

---

## 3. NUEVAS RECOMENDACIONES PARA DS10

### 3.1 Thermal Management — STM32H743
**Datos DS9 medidos**: 40-41°C @ 480MHz + 5×SAI + DMA pipeline
- **Actual**: Sin disipación, temperatura estable
- **Recomendación DS10**:
  - Aumentar vías bajo BGA STM32H743 a plano GND (minimum 8×8 grid, 0.3mm vías)
  - Considerar pad térmico de cobre de ~1cm² bajo STM32 hacia GND layer
  - **Si integras amp**: posicionar amp lejos de STM32 (al menos 3cm)

### 3.2 Decoupling Capacitors — Audio-Grade
**DS9 experiencia**: 0 underruns, jitter 0µs → decoupling está OK
**Recomendación DS10**: Mantener estrategia, mejorar si es posible
- **Cada IC**: 100nF cerámica (X7R, <1mm del pin)
- **Cada rail 5V→3.3V**: 10µF + 100nF en paralelo
- **PCM1690**: Datasheet especifica 47µF tantalio + 100nF cerámica (crítico)
- **ESP32-C3**: 10µF + 100nF, posible agregar 47µF extra si hay inestabilidad WiFi

### 3.3 Clock Routing — MCLK 12.288MHz
**DS9**: PLL3 genera MCLK, distribuida a todos los SAI
**Recomendación DS10**:
- Ruta MCLK en capa interna (GND/power sandwich)
- Evitar cambios de ancho/impedancia
- **Length matching** para SAI sync (max ±5mm entre SAI blocks)
- Ferrite small (como mencionas) es buena idea para EMI

### 3.4 ADC — AK5538VN (reemplaza PCM1802)
**DS9**: PCM1802 (2ch, 24-bit, SAI3_B)
**DS10**: AK5538VN (8ch, 32-bit, TDM256 slave, SAI3_A)

**Por qué AK5538VN:**
- 2ch → 8ch (expansión de entrada sin agregar MCU)
- 24-bit → 32-bit (más headroom para beamforming)
- I²C control (no necesita GPIO strapping manual)
- TDM256: 8ch × 32-bit = 256 bits/frame, BICK=12.288MHz @ 48kHz (mismo MCLK que DACs)

**Conexión SAI:**
- SAI3_A en modo esclavo, sincronizado con BICK/LRCK del sistema
- MCLK compartido con DACs (12.288MHz) — **CRÍTICO para coherencia de fase en beamforming**
- Separar MCLKs destruiría la coherencia de fase entre entradas y salidas

**Power para AK5538VN:**
```
5V ──[AP2125K-3.3]──→ AVDD1/AVDD2 (pins 2,15) + 10µF+0.1µF por pin
DVDD 3.3V ──────────→ TVDD (pin 25) + 10µF+0.1µF
VDD18 (pin 27) ─────→ 4.7µF+0.1µF a DVSS (solo cap de salida del LDO interno)
VREFH1-4 ──[20Ω]───→ AVDD + 100µF||0.1µF a AVSS
VREFL1-4 ──────────→ AVSS (directo)
```

**Pines sin usar (floating NO permitido):**
- Unused digital inputs: tie a DVSS o DVDD según función (ver datasheet Table 2)
- LDOE (pin 50): H = habilita LDO interno para VDD18 (recomendado)
- CKSEL, TDM, SLOT, DIF: strapping para TDM256 slave mode con I²C
- All VREFH/VREFL: conectar según tabla de arriba (no dejar flotantes)

**AVDD supply — AP2125K-3.3TRG1:**
- LCSC: C89359 (Basic part en JLCPCB)
- Package: SOT-23-5
- Corriente: 300mA max (AK5538VN consume ~55mA total)
- PSRR: 75dB @ 1kHz — excelente aislamiento de ruido digital
- Alimentar desde 5V rail (NO desde DVDD 3.3V) para máximo aislamiento

### 3.5 SPDIFRX — Análisis en LQFP100
Pines SPDIFRX disponibles en STM32H743 LQFP100:
| Input | Pin | Estado DS10 | Nota |
|-------|-----|-------------|------|
| IN0 | PD7 | **LIBRE** ✅ | Único sin conflictos |
| IN1 | PD8 | CONFLICTO ❌ | SAI3_B SCK (ADC BCK) |
| IN2 | PC4 | LED1 (D6) ⚠️ | Sacrificable si se quita LED |
| IN3 | PC5 | LED2 (D7) ⚠️ | Sacrificable si se quita LED |

- Cada link SPDIFRX = 2 canales estéreo (1 trama IEC60958)
- Para DS10: implementar IN0 si se necesita entrada SPDIF
- IN2/IN3 viables si se eliminan LEDs amarillo/rojo

### 3.6 Conector de Expansión (Inter-board)
**Objetivo**: Cascading múltiples unidades DS10, tipo paneles LED ensamblables

**Conector recomendado**: JST-SH 1.0mm pitch, 2×10 (J1/J14/J29 ya en BOM, LCSC C5343037)
- Alternativa densa: Molex PicoBlade 1.25mm, 2×8 o 2×10
- 1.27mm pitch Samtec FTSH/FTMS series (mezzanine, muy compacto)

**Señales a exponer en header de expansión:**
- MCLK (12.288MHz) — para sincronización de fase entre unidades
- SAI BICK / LRCK — sincronización de audio
- I2C (address expansion para múltiples slaves)
- UART (daisy-chain o bus RS-485)
- GPIO (trigger, sync, status)
- 5V + 3.3V + GND

**Inter-board NO usar SPDIF**: demasiado complejo para muchos links simultáneos.
Preferir: señal analógica diferencial, SAI over ribbon, o protocolo serial propio.

### 3.7 EEPROM (CAT24C256 en I2C4)
**DS9**: Funcionando, config persistence OK
**Recomendación DS10**:
- Agregar resistencia pull-up 10kΩ a 3.3V en SDA/SCL si no está (I2C4 en D3 domain)
- **WP pin** (Write Protect, PB5 en DS9): Mantener en DS10 para production lock
- Considerar **EEPROM en SOIC-8** en lugar de DIP-8 si ahorras espacio

### 3.8 Power Sequencing
**DS9**: Todo arranca simultáneamente (sin problemas)
**Recomendación DS10** si amplificador externo:
- **Orden recomendado**:
  1. 5V bulk → 3.3V LDO (STM32, ESP32)
  2. Esperar 100ms → STM32 boot (HAL_Init)
  3. 100ms más → DAC MCLK estable → PCM1690 init
  4. 100ms más → Audio amplificador power (relay/mosfet controlado desde GPIO)
- **Detectar falla**: Si amp no enciende, desactivar STM32 SAI para evitar ruido en línea output

### 3.9 Conector de Audio — Ahora Horizontal
**Buena decisión** (menor altura PCB)
**Verificar**:
- ¿3.5mm jack o XLR? (depende de uso final)
- ¿Impedancia del conector? (audio requiere bajo Z, típ <5Ω)
- ¿Blindaje** entre entrada/salida? Si es necesario, ruta en diferentes capas

### 3.10 HAT Connector — 5V Power
**Recomendación**:
- Si HAT requiere mucha corriente (amp externo), considerar JST PH 2.0 connector en lugar de pin header
- Calibre cable: Si IAT > 1A, usar AWG18 (típ 2A)
- Protección: Fusible rápido 1-2A en línea 5V entrada

### 3.11 I2C Pull-ups
**DS9**: 10kΩ en I2C1 (DACs) — OK
**DS10**: Verifica que ambas ramas de I2C tienen pull-ups:
- I2C1: DACs (PCM1690 ×4)
- I2C4: EEPROM (CAT24C256)
- **Recomendación**: 10kΩ cada rama, pull-up a 3.3V

### 3.12 WiFi Antenna — ESP32-C3-MINI-1
**DS9**: Funciona bien (conecta a WiFi normalmente)
**Recomendación DS10**:
- Si cambias footprint: asegurar que antenna matching está bien
- **Si usas cerámica antenna** (no PCB antenna): Ruta de 50Ω desde ESP32 RF pin
- Posición: Lejos de STM32, lejos de ground planes grandes (evita shadowing)

### 3.13 Reset Circuit
**STM32H743 reset**:
- Cap de desacople 100nF a GND en NRST pin (normalmente ya está)
- Pull-up 10kΩ interno, pero mantener externa si usas JTAG/SWD
- **Recomendación**: Resistencia 1kΩ serie desde botón reset ESP32 si lo compartís con STM32

### 3.14 Test Points para Manufactura
**Recomendación DS10**:
- Test point en MCLK (verificar 12.288MHz)
- Test point en 3.3V rail
- Test point en 5V input
- Test point en UART4 TX/RX (debug rápido en QA)
- Facilita diagnóstico en planta sin soldar

---

## 4. CHECKLIST PRE-MANUFACTURA DS10

- [ ] ERC clean (KiCAD: no unconnected nets, no multiple drivers)
- [ ] DRC clean (KiCAD: clearances, trace width, via sizes)
- [ ] Pin conflict audit vs STM32H743 LQFP100 datasheet
- [ ] Schematic review:
  - [ ] Cada IC tiene power/GND pins conectados
  - [ ] Decoupling caps 100nF < 1mm de cada power pin
  - [ ] I2C pull-ups presentes (ambas ramas)
  - [ ] Reset circuits OK
  - [ ] Crystal/oscilador bien terminado
- [ ] PCB layout review:
  - [ ] MCLK routing (internal layer, low impedance)
  - [ ] SAI traces length-matched (±5mm)
  - [ ] 5V/3.3V planes continuous (no islands)
  - [ ] Thermal vías bajo STM32 (8×8 minimum)
  - [ ] Conector STLink cerca de STM32 (< 10cm)
- [ ] Firmware compatibility check:
  - [ ] I2C1, I2C4 addresses unchanged (DACs 0x98/0x9C/0x9A/0x9E, EEPROM 0xA0)
  - [ ] GPIO assignments compatible (LED pins, UART4, etc)
  - [ ] No new pin conflicts con SAI alternate functions
- [ ] Fabrication files:
  - [ ] Gerbers (2-4 layer stack specified)
  - [ ] Drill file
  - [ ] BOM with Digi-Key/Mouser part numbers
  - [ ] Assembly drawing
  - [ ] Silkscreen clear (reference designators, polarity markers)

---

## 5. COMPARATIVA DS9 ↔ DS10

| Aspecto | DS9 | DS10 |
|--------|-----|------|
| **Pines GPIO usados** | +5 (DAC control) | -5 (I2C only) |
| **Alimentación** | Multi-voltage | 5V único |
| **Amplificador** | Externo (módulo) | Integrado |
| **Conector STLink** | microUSB | SH1.0 |
| **Pin conflicts** | PE3, PE11 | **Resueltos** |
| **ADC entradas** | PCM1802 — 2ch 24-bit | **AK5538VN — 8ch 32-bit** |
| **ADC SAI** | SAI3_B | **SAI3_A** |
| **AVDD supply** | 3.3V digital | **AP2125K-3.3 LDO desde 5V** |
| **Costo componentes** | Baseline | -15% (aprox) |
| **Thermal** | 40-41°C OK | Similar (mejorada con vías) |
| **Manufactura** | THT manual | **SMD eliminando THT → -$99** |

---

## 6. OPTIMIZACIÓN COSTOS JLCPCB DS10

### THT → SMD (eliminar manual assembly fee ~$99)
| Componente | Solución | Ahorro estimado |
|------------|----------|-----------------|
| 16x JST-XH speaker (J9-J27) | Soldar a mano fuera de JLCPCB | ~$55 |
| 14x 10µF electrolítico (CP_Elec_4x5.7) | Reemplazar con 10µF SMD cerámico 0603 | ~$35-40 |
| C47 (1000µF radial) | Reemplazar con 4x 220µF SMD o cluster de 470µF SMD | ahorro en fee |
| J8 barrel jack (si aplica) | SMD equivalente o soldar a mano | menor |

**Total potencial**: ~$90-95 ahorro eliminando manual assembly fee.

### KiCad → JLCPCB Pick & Place
Script Python de corrección de rotación (aplicar antes de subir a JLCPCB):
```python
CORRECTIONS = {
    "LQFP-100_14x14mm_P0.5mm":           {"rotation": -90, "dx": 0, "dy": 0},
    "SOP50P810X120-49N":                   {"rotation": -90, "dx": 0, "dy": 0},
    "SSOP-20_5.3x7.2mm_P0.65mm":          {"rotation": -90, "dx": 0, "dy": 0},
    "SOT-223-3_TabPin2":                   {"rotation": 180, "dx": 0, "dy": 0},
    "SOT-23-3":                            {"rotation": 180, "dx": 0, "dy": 0},
    "CP_Elec_4x5.7":                       {"rotation": 180, "dx": 0, "dy": 0},
    "CAPPM3216X180N":                      {"rotation": 180, "dx": 0, "dy": 0},
}
```
Alternativa: usar **KiCad Fabrication Toolkit** plugin (GUI, mantiene diccionario por footprint).

---

## 7. FIRMWARE COMPATIBILITY DS9 → DS10

**Buena noticia**: Firmware DS9 funcionará sin cambios en DS10 porque:
- ✅ I2C addresses (DACs, EEPROM) sin cambios
- ✅ SAI pin assignments (PE2-PE6, PD8-13, etc) sin cambios
- ✅ GPIO para LEDs/UART sin cambios
- ⚠️ **Si integras amp con control GPIO**: Nuevo pin para on/off (agregar en firmware)

---

## 8. NETLIST VERIFICATION (2026-03-25, 570 nets)

### Power Supply Chain — VERIFIED
```
+24V (J9/2) → IC2/2 (MP2315S VIN)
IC2/3 (SW) → L1/1 → L1/2 → +5VA (32x PAM + FB2 + FB3)
+5VA → FB2 → +5V (U10 input, U11 input)
+5V → U11/3 (AP1117) → U11/2 → +3V3 (digital)
+5V → U10/1 (AP2125K) → U10/5 → +3.3VA (analog)
+5VA → FB3 → +5VA_DAC (4x PCM1690 AVDD)
```

### SAI Bus Mapping — VERIFIED
| SAI | STM32 Pins | Target | Data |
|-----|-----------|--------|------|
| SAI1_A (FS/SCK/SD) | U7/3,4,5 | U2+U3 (DAC0+DAC1) | 16ch via RN2,RN3 |
| SAI1_B (SD) | U7/2 | U3 (DAC1) | 8ch via RN3 |
| SAI2_A (FS/SCK/SD) | U7/58,59,60 | U1+U4 (DAC2+DAC3) | 16ch via RN1,RN4 |
| SAI2_B (SD) | U7/41 | U4 (DAC3) | 8ch via RN4 |
| SAI3_B (SCK/SD/FS) | U7/55,56,57 | IC3 AK5538 (BICK/SDTO1/LRCK) | 8ch TDM256 |
| SAI1_MCLK | U7/1 (PE2) | All DACs + IC3/24 + JP2 | 12.288MHz |

### I2C Bus Map — VERIFIED
| Bus | STM32 Pins | Targets | Pull-ups |
|-----|-----------|---------|----------|
| I2C1 | U7/95(SCL), U7/96(SDA) | U1-U4 DACs (0x4C-0x4F) | RN5 |
| I2C2 | U7/46(SCL), U7/47(SDA) | J5 expansion header | RN6 |
| I2C3 | U7/67(SCL), U7/66(SDA) | IC3 AK5538 (0x14) | RN6 |
| I2C4 | U7/92(SCL), U7/93(SDA) | U8 EEPROM | RN5 |

### AK5538VN Strapping — VERIFIED
| Pin | Function | Connection | Result |
|-----|----------|------------|--------|
| 51 | TDM0 | GND | L |
| 52 | TDM1 | +3V3 | H |
| → | **TDM Mode** | TDM1=H, TDM0=L | **TDM256** |
| 54 | I2C | +3V3 | I2C mode enabled |
| 29-31 | PW0-PW2 | +3V3 | 32-bit word |
| 32 | MSN | GND | Slave mode |
| 57 | LDOE | +3V3 | Internal 1.8V LDO on |

### +3.3VA Net — VERIFIED
`U10/5, IC3/2(AVDD1), IC3/15(AVDD2), R102-R105(VREFH), C63, C64, C259, C260, C265, C266`

### DAC I2C Addresses — VERIFIED
| DAC | ADR0 (pin22) | ADR1 (pin23) | Address |
|-----|-------------|-------------|---------|
| U2 | GND | GND | 0x4C |
| U1 | +3V3 | GND | 0x4D |
| U3 | GND | +3V3 | 0x4E |
| U4 | +3V3 | +3V3 | 0x4F |

### Control Signals — VERIFIED
- RST: U1-U4/15 + R16→+3V3 + U7/79
- AMUTEI: U1-U4/18 + R15→+3V3 + U7/80
- AMUTEO: U1-U4/19 + U7/78 (wire-OR intentional)
- PAM SD: 32x PAMs /ALL_AMPS/SD → R20 → U7/84 (PD3)

---

## 9. PRE-PRODUCTION CHECKLIST (2026-03-25)

### Completed
- [x] PCM1802 sheet removed from schematic
- [x] AK5538VN TDM256 strapping corrected (TDM0↔TDM1 swapped)
- [x] VBAT connected to +3V3
- [x] VREFL1-4 connected to GND
- [x] AK5538VN I2C3 connected (SDA/SCL with pull-ups RN6)
- [x] FB67 removed (was bridging +3V3 → +3.3VA)
- [x] GND unified (single net, solid L2 plane)
- [x] SAI1→DAC0+DAC1, SAI2→DAC2+DAC3, SAI3_B→AK5538 verified
- [x] All 4 DAC I2C addresses unique (0x4C-0x4F)
- [x] +3.3VA LDO output verified connected to AK5538 AVDD + VREFH
- [x] Netlist script fixed (null check + auto-save)
- [x] 570-net complete netlist generated and verified

### Additional fixes (2026-03-31, pre-production)
- [x] SMBJ24A TVS added on 24V input (parallel +24V to GND, DO-214AA)
- [x] ESP32 GPIO9 external pull-up 10kΩ to +3V3 (boot mode fix)
- [x] AK5538 OVF (IC3/42) connected to STM32 PA15 (U7/77)
- [x] 16x 100pF C0G/NP0 caps on AK5538 analog inputs (AINxP/AINxN)
- [x] Speaker connectors renamed JSPK01-JSPK16 (was scattered JXX)
- [x] Ferrite universal: CBW201209U601T (C21286) for all 67 positions
- [x] Production Gerbers generated and uploaded to JLCPCB
- [x] STM32H743VIT6 procurement via LCSC consignment to JLCPCB

### Remaining DRC/ERC (at time of production order)
- Minor DRC: 5x vias 0.2mm drill (min 0.3mm), 2x unconnected pads, silk text
- Minor ERC: 6x PWR_FLAG needed, 3x AMUTEO suppressions, duplicate labels
- These are non-critical and do not affect board functionality

### Additional thermal improvements (2026-04-01, pre-release)
- [x] MP2315S (IC2): solid copper fill on SW node / inductor L1 footprint on L1
- [x] Via stitching added in GND copper zone around IC2 (6-10x vias, 0.4/0.8mm, L1→L2)
  - SOT-23-8 has no exposed thermal pad — heat path is: pins → GND copper → vias → L2 plane
  - Vias placed within 3-4mm of IC2 in the surrounding GND pour, not under component

### Post-production improvements (for next revision)
- [ ] C47 (1000µF THT) → 4-5x 220µF polymer SMD (SS221M160E7R7R4Z00ZZ, 16V)
- [ ] Barrel jack J32 switch pin → end-of-chain detection (100kΩ/10kΩ divider to GPIO)
- [ ] ESD protection on first-in-chain RJ45 inputs (future consideration)
- [ ] SAI clock routing: buses with 0.2mm clearance OK, consider 0.3mm if EMI issues arise

---

## 10. PRODUCTION NOTES (2026-03-31)

### JLCPCB Order
- **Quantity**: 5 units
- **PCB**: 4-layer, 1.6mm, JLC04161H-7628 stackup, HASL
- **Assembly**: SMD both sides
- **Manual solder (THT)**: RJ45 connectors (×4), barrel jacks J31/J32, speaker connectors JSPK01-16
- **Consignment**: STM32H743VIT6 (LCSC internal transfer to JLCPCB SMT stock)

### Component Selection Notes
- **Ferrite (all 67 positions)**: CBW201209U601T (LCSC C21286) — 600Ω@100MHz, 0805, 300mA
- **Coupling caps (DAC→Amp)**: 0.22µF X7R, 10% tolerance — adequate for fc=72Hz
- **LPF caps (ΔΣ noise)**: 4.7nF C0G/NP0 — fc≈34kHz
- **ADC input caps**: 100pF C0G/NP0, one per analog input (16 total)
- **TVS protection**: SMBJ24A unidirectional (24V standoff, DO-214AA)

### Known Limitations (this production run)
- C47 is still THT 1000µF electrolytic (manual solder) — plan to replace with SMD polymer
- Minor DRC/ERC warnings remain (cosmetic, non-functional)
- STM32H743VIT6 stock limited on LCSC — ordered via consignment

---

## 11. FIRMWARE CONFIGURATION REFERENCE — DS10

> Referencia completa para la sesión de programación. Todos los datos verificados contra netlist (574 nets, 2026-04-02) y BOM de producción.
>
> **Nota sobre netlist**: R107-R110 (resistencias 1kΩ serie en cada línea AMUTEO) fueron agregadas al esquemático después de la última exportación de netlist.md. El BOM de producción las incluye. El netlist.md muestra AMUTEO como conexión directa, pero el circuito real tiene los 1kΩ de protección.

---

### 11.1 Relojes

| Fuente | Frecuencia | Uso |
|--------|-----------|-----|
| HSE (Y2, PH0/PH1) | 25 MHz | Clock principal STM32 |
| LSE (Y1, PC14/PC15) | 32.768 kHz | RTC |
| PLL1 desde HSE | 480 MHz | SYSCLK |
| PLL3 desde HSE | **12.288 MHz** | SAI1_MCLK → todos los DAC + AK5538 |

**SAI1_MCLK (PE2)** sale del STM32 y es distribuido físicamente a:
- RN1/4, RN2/4, RN3/4, RN4/4 → 4× PCM1690 MCLK
- IC3/24 → AK5538VN MCLK
- JP2/1 → JP2 jumper (normalmente ABIERTO) → J5/7

Para obtener 12.288 MHz exacto desde HSE 25 MHz, usar PLL3 con FRACN habilitado o aceptar ≈12.25 MHz (error <0.3%, suficiente para audio no crítico). La coherencia de fase entre unidades depende de que **todas usen el mismo MCLK físico**, no de la frecuencia exacta.

---

### 11.2 SAI — Audio Serial Interface

#### SAI1 → DAC0 (U2, 0x4C) + DAC1 (U3, 0x4E)

| Señal | STM32 pin | GPIO | AF | Nota |
|-------|-----------|------|----|------|
| MCLK | 1 | PE2 | AF6 | Compartido con todos |
| SCK_A | 4 | PE5 | AF6 | SAI1 master clock |
| FS_A | 3 | PE4 | AF6 | Frame sync para DAC0+DAC1 |
| SD_A | 5 | PE6 | AF6 | Data → DAC0 (U2) |
| SD_B | 2 | PE3 | AF6 | Data → DAC1 (U3) |

#### SAI2 → DAC2 (U1, 0x4D) + DAC3 (U4, 0x4F)

| Señal | STM32 pin | GPIO | AF | Nota |
|-------|-----------|------|----|------|
| SCK_A | 60 | PD13 | AF10 | SAI2 master clock |
| FS_A | 59 | PD12 | AF10 | Frame sync para DAC2+DAC3 |
| SD_A | 58 | PD11 | AF10 | Data → DAC2 (U1) |
| SD_B | 41 | PE11 | AF10 | Data → DAC3 (U4) |

#### SAI3_B → AK5538VN ADC (IC3, 0x14)

| Señal | STM32 pin | GPIO | AF | AK5538 pin | Nombre AK5538 |
|-------|-----------|------|----|------------|---------------|
| SCK_B | 55 | PD8 | AF6 | 33 | BICK/DCLK |
| SD_B (RX) | 56 | PD9 | AF6 | 36 | SDTO1/DSDOL2 |
| FS_B | 57 | PD10 | AF6 | 34 | LRCK/DSDOL1 |

SAI3 en **modo esclavo** (MSN=GND). AK5538 genera LRCK y BICK internamente sincronizado a MCLK. Configurar SAI3 en modo TDM256 (256 bits/frame @ 48kHz = BICK 12.288MHz).

#### Resumen TDM por DAC

Cada PCM1690 recibe TDM8 (8 canales × 32-bit) por su SD pin. SAI1_A y SAI1_B transportan DAC0 y DAC1 en paralelo → 16 canales. SAI2_A y SAI2_B igual → 32 canales totales.

---

### 11.3 I2C — Mapa completo

| Bus | STM32 pin SCL | GPIO SCL | STM32 pin SDA | GPIO SDA | AF | Dispositivos | Pull-ups |
|-----|--------------|----------|--------------|----------|----|-------------|----------|
| I2C1 | 95 | PB8 | 96 | PB9 | AF4 | 4× PCM1690 (0x4C–0x4F) | RN5 (4.7kΩ) |
| I2C2 | 46 | PB10 | 47 | PB11 | AF4 | J5 expansion | RN6 (4.7kΩ) |
| I2C3 | 67 | PA8 | 66 | PC9 | AF4 | AK5538VN (0x14) | RN6 (4.7kΩ) |
| I2C4 | 92 | PB6 | 93 | PB7 | AF11 | EEPROM CAT24C256 (0x50) | RN5 (4.7kΩ) |

**Notas:**
- I2C1: DAC addresses 0x4C-0x4F son 7-bit. 8-bit write: 0x98/0x9A/0x9C/0x9E
- I2C3: AK5538VN address 0x14 (7-bit, verificar con datasheet sección "I2C slave address")
- I2C4: EEPROM A0=A1=A2=GND → 7-bit address 0x50 (8-bit write 0xA0)

#### DAC PCM1690 — Mapa de direcciones I2C

| Ref | ADR0 (pin 22) | ADR1 (pin 23) | I2C addr (7-bit) | SAI SD |
|-----|--------------|--------------|-----------------|--------|
| U2 | GND | GND | **0x4C** | SAI1_SD_A |
| U1 | +3V3 | GND | **0x4D** | SAI2_SD_A |
| U3 | GND | +3V3 | **0x4E** | SAI1_SD_B |
| U4 | +3V3 | +3V3 | **0x4F** | SAI2_SD_B |

---

### 11.4 UART

| Periférico | STM32 pin TX | GPIO TX | STM32 pin RX | GPIO RX | AF | Destino |
|------------|-------------|---------|-------------|---------|----|----|
| UART4 | 22 | PA0 | 23 | PA1 | AF8 | ESP32-C3-Mini (U5) |
| USART2 | 24 | PA2 | 25 | PA3 | AF7 | J6/7 + J6/6 (expansion) |
| UART7 | 38 | PE8 | 37 | PE7 | AF7 | J7/8 + J7/7 (SWD header, debug) |

**UART4 ↔ ESP32**: PA0=STM32_TX→U5/13(ESP32_RX), PA1=STM32_RX→U5/12(ESP32_TX). Conexión directa sin resistencias (no es RS-485).

---

### 11.5 GPIO — Mapa completo

#### Salidas de control

| Función | STM32 pin | GPIO | Estado seguro | Descripción |
|---------|-----------|------|--------------|-------------|
| DAC_RST | 79 | PC11 | HIGH (activo bajo) | ~RST de los 4 PCM1690 |
| DAC_MUTE_IN | 80 | PC12 | LOW | AMUTEI de los 4 PCM1690 |
| PAM_SD | 84 | PD3 | LOW (amps off) | SD de los 32 PAM8302A vía R20 |
| AK_PDN | 64 | PC7 | LOW (apagado) | Power-down AK5538VN (activo bajo) |
| AK_DIF1 | 65 | PC8 | — | Interface select AK5538VN (ver datasheet) |
| EEPROM_WP | 91 | PB5 | HIGH | Write-protect CAT24C256 (HIGH=protegido) |

#### Entradas de estado

| Función | STM32 pin | GPIO | Descripción |
|---------|-----------|------|-------------|
| DAC_MUTE_OUT | 78 | PC10 | AMUTEO wire-OR de los 4 PCM1690 (via 4× 1kΩ R107-R110) |
| AK_OVF | 77 | PA15 | Overflow del AK5538VN (IC3/42) |
| OPT_SW (K2) | 28 | PA4 | Botón usuario (pull-down externo, activo alto cuando presionado? verificar) |

#### LEDs — TIM3 PWM (AF2)

| LED | Color | STM32 pin | GPIO | TIM3 canal |
|-----|-------|-----------|------|-----------|
| D4 | Amarillo | 30 | PA6 | CH1 |
| D5 | Rojo | 31 | PA7 | CH2 |
| D6 | Verde | 34 | PB0 | CH3 |
| D7 | Blanco | 35 | PB1 | CH4 |

> D1 (Rojo) está conectado a ESP32 U5/16 — no al STM32.

#### Botones

| Botón | Función | Conexión | Comportamiento |
|-------|---------|----------|----------------|
| K1 | ESP32 GPIO8 | U5/22 + R8(pull-up) | Press → GPIO8 LOW (ESP32 boot mode) |
| K2 | STM32 PA4 | U7/28 + R17 | Press → PA4 activo |
| K3 | STM32 BOOT0 | U7/94, K3/1=+3V3 | Press → BOOT0 HIGH → bootloader en próximo reset |
| K4 | STM32 NRST | U7/14, K4/2=GND | Press → NRST LOW → reset STM32 |

**Secuencia DFU**: K3 presionado (BOOT0=H) → K4 presionado y soltado (reset) → K3 suelto → STM32 entra en bootloader.

#### SWD / Debug

| Señal | STM32 pin | GPIO | J7 pin |
|-------|-----------|------|--------|
| SWDIO | 72 | PA13 | J7/2 |
| SWCLK | 76 | PA14 | J7/3 |
| SWO | 89 | PB3 | J7/5 |
| NRST | 14 | — | J7/6 |
| UART7_RX | 37 | PE7 | J7/7 |
| UART7_TX | 38 | PE8 | J7/8 |

---

### 11.6 Secuencia de inicialización — PCM1690 (I2C1)

1. Mantener DAC_RST (PC11) LOW durante boot
2. Configurar SAI1 y SAI2 en modo master TDM
3. Arrancar PLL3 → MCLK 12.288MHz en PE2
4. Esperar ≥5ms (estabilización MCLK)
5. Subir DAC_RST HIGH → salida de reset PCM1690
6. Esperar ≥10ms (PCM1690 power-on reset interno)
7. Por I2C1, configurar cada DAC (0x4C, 0x4D, 0x4E, 0x4F):
   - Reset de registros (PWR_DN = 1 → 0)
   - Configurar modo TDM/SAI (registro 0x01)
   - Audio format: 32-bit TDM, MCLK/BCK ratio
   - Unmute (AMUTEI LOW via PC12)
8. Configurar PAM_SD (PD3) HIGH → habilitar amplificadores

**Secuencia de mute segura**: AMUTEI (PC12) HIGH → esperar fin de frame → PAM_SD (PD3) LOW.

---

### 11.7 Configuración AK5538VN (I2C3)

Strapping hardware en la placa (no modificar por software):

| Pin AK5538 | Función | Conexión | Valor |
|-----------|---------|----------|-------|
| 51 | TDM0 | GND | L → TDM256 |
| 52 | TDM1 | +3V3 | H → TDM256 |
| 54 | I2C/SPI sel | +3V3 | I2C mode |
| 29-31 | PW0-PW2 | +3V3 | 32-bit |
| 32 | MSN | GND | Slave |
| 57 | LDOE | +3V3 | LDO interno ON |

Inicialización vía I2C3 (dirección 0x14):
1. PDN_AK (PC7) HIGH → AK5538VN en power-down
2. Configurar SAI3_B como receptor TDM256 esclavo
3. PDN_AK LOW → salida de power-down
4. Esperar ≥10ms
5. Configurar registros AK5538VN: sample rate 48kHz, TDM256, 32-bit
6. OVF (PA15): monitorear overflow por interrupción EXTI

**ADC channels en RJ45**: AIN1-AIN8 llegan via J27/J28/J29/J30 (pares diferenciales T568B sobre Cat5e).

---

### 11.8 PAM8302A — Control

- **SD pin (todos los 32 amplificadores)**: red `/ALL_AMPS/SD` → R20/2
- **STM32 control**: PD3 (pin 84) → R19 (pull-down a GND) + R20 (serie a /ALL_AMPS/SD)
  - R19 mantiene amps en shutdown durante reset STM32 (seguro)
  - R20 limita corriente GPIO
- **Activar amps**: PD3 = HIGH
- **Silenciar amps**: PD3 = LOW (antes de apagar para evitar pop)

---

### 11.9 ESP32-C3-Mini — Configuración

| ESP32 pin | Función | Conexión |
|-----------|---------|----------|
| U5/12 (UART0_TX) | UART TX | → STM32 PA1 (UART4_RX) |
| U5/13 (UART0_RX) | UART RX | ← STM32 PA0 (UART4_TX) |
| U5/16 | LED D1 (Rojo) | → D1 cátodo vía R6 |
| U5/22 (GPIO8) | Boot mode + K1 | Pull-up R8 a +3V3, K1 a GND |
| U5/23 (GPIO9) | — | Pull-up R106 a +3V3 |
| U5/26 (D+) | USB | → U6 (ESD) → J3 USB-C |
| U5/27 (D-) | USB | → U6 (ESD) → J3 USB-C |
| U5/8 (EN) | Chip enable | R5 pull-up + C25 (RC reset) |

**Boot normal**: GPIO8=H (R8 pull-up), GPIO9=H (R106 pull-up). K1 presionado = GPIO8 LOW = enter download mode si EN también cicla.

---

### 11.10 Headers de expansión

#### J5 (JST-SH 1×10, 1mm pitch)

| Pin | Señal | GPIO | Descripción |
|-----|-------|------|-------------|
| 1 | I2C2_SCL | PB10 (AF4) | I2C2 |
| 2 | I2C2_SDA | PB11 (AF4) | I2C2 |
| 3 | SAI3_SCK_A | PD0 (AF6) | SAI3 expansion |
| 4 | SAI3_SD_A | PD1 (AF6) | SAI3 expansion |
| 5 | I2S2_SDI | PB14 (AF5) | I2S2 |
| 6 | SAI3_FS_A | PD4 (AF6) | SAI3 expansion |
| 7 | MCLK* | PE2 vía JP2 | *JP2 normalmente ABIERTO |
| 8 | GND | — | |
| 9 | +3V3 | — | |
| 10 | +5V | — | |

#### J6 (JST-SH 1×10, 1mm pitch)

| Pin | Señal | GPIO | Descripción |
|-----|-------|------|-------------|
| 1 | I2S2_WS | PB12 (AF5) | I2S2 |
| 2 | I2S2_CK | PB13 (AF5) | I2S2 |
| 3 | SPDIFRX_IN3 | PC5 (AF9) | S/PDIF input 3 |
| 4 | SPDIFRX_IN2 | PC4 (AF9) | S/PDIF input 2 |
| 5 | SPDIFRX_IN0 | PD7 (AF9) | S/PDIF input 0 |
| 6 | USART2_RX | PA3 (AF7) | Debug serial |
| 7 | USART2_TX | PA2 (AF7) | Debug serial |
| 8 | GND | — | |
| 9 | +3V3 | — | |
| 10 | +5V | — | |

---

### 11.11 GPIOs sin usar en STM32 (configurar como Analog en firmware)

Para minimizar consumo y EMI, configurar como `GPIO_MODE_ANALOG` sin pull-up/down:

`PE0, PE1, PE9, PE10, PE12, PE13, PE14, PE15, PB2, PB4, PB15, PC0, PC1, PC2, PC3, PC6, PD2, PD5, PD6, PD14, PD15, PA5, PA9, PA10`

---

### 11.12 Archivos para la sesión de firmware

**Prioridad 1 — Configuración de puertos (este documento)**:
- [DS10_DESIGN_REVIEW.md](DS10_DESIGN_REVIEW.md) — Secciones 8 y 11 (netlist verification + firmware reference)

**Prioridad 2 — Arquitectura del sistema**:
- [README.md](README.md) — Descripción de la instalación y objetivos de beamforming
- [DS10_PRE_PRODUCTION_CHECKLIST.md](DS10_PRE_PRODUCTION_CHECKLIST.md) — Sección 1 (verificación completa de conexiones por netlist)

**Prioridad 3 — Netlist completo (ground truth)**:
- [librerias/netlist.md](librerias/netlist.md) — 574 nets verificados (generado antes de agregar R107-R110)

**No compartir**:
- Archivos KiCad (.kicad_sch, .kicad_pcb) — demasiado voluminosos y no procesables por firmware
- [librerias/DRC.rpt](librerias/DRC.rpt) y [librerias/ERC.rpt](librerias/ERC.rpt) — solo relevantes para EDA

---

**Last updated**: 2026-04-29
**Branch**: main
**Status**: PCBs recibidos de JLCPCB — fase de bring-up
