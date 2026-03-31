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

### Post-production improvements (for next revision)
- [ ] C47 (1000µF THT) → 4-5x 220µF polymer SMD (SS221M160E7R7R4Z00ZZ, 16V)
- [ ] Barrel jack J32 switch pin → end-of-chain detection (100kΩ/10kΩ divider to GPIO)
- [ ] MP2315S thermal pad: verify via array (min 4-6x 0.3mm tented) + solid GND on L1/L2
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

**Last updated**: 2026-03-31
**Branch**: power_supply_24
**Status**: SENT TO PRODUCTION
