# DS9 v2.0 Pin Audit — SAI Errors & Bodge Wires

> **HISTÓRICO — DS9 únicamente.** Los errores documentados aquí fueron corregidos en el diseño DS10.
> DS10 usa los mismos pines SAI correctos (PE2-PE6/SAI1, PD11-PD13/PE11/SAI2, PD8-PD10/SAI3_B),
> con la diferencia de que el ADC es AK5538VN en SAI3_B (no PCM1802/SAI4_B).
> Ver [ds10_pin_map.md](ds10_pin_map.md) para el mapa actual.

**Fecha**: 2026-02-12 (resultados bringup agregados 2026-02-18)
**Source**: STM32H743VI datasheet DS12110 Rev 10, Tables 13-14 (páginas 93-94)
**PCB**: DS9 v2.0 — 5 boards fabricadas (JLCPCB, recibidas 2026-02-18)

## Summary

| Peripheral | Status | Issue |
|-----------|--------|-------|
| SAI1_A (DAC0, U3) | **BROKEN** | PE7/PE8/PE9 have NO SAI function |
| SAI1_B (DAC1, U4) | **BROKEN** | PE3=SAI1_SD_B OK, but PE4/PE5 conflict (used by SAI1_A currently) |
| SAI2_A (DAC2, U2) | **WORKS** (labels swapped) | PD12=FS, PD13=SCK (pin_definitions.h has them inverted) |
| SAI2_B (DAC3, U8) | **BROKEN** | PD8/PD9/PD10 are SAI3_B (AF6), NOT SAI2_B (AF10) |
| SAI4_B (ADC, U1) | **BROKEN** | PC0/PC1/PB11 wrong AF mapping |
| **Working channels** | **8-16** | DAC2 via SAI2_A definitely works. DAC1 possible if SAI1_A stays on PE4/PE5/PE6 |

## DS9 Bringup Results (2026-02-18)

**I2C scan with MCLK fix — all 4 PCM1690 found:**

| Device | I2C Addr | Bus | Status |
|--------|----------|-----|--------|
| PCM1690 DAC0 (U3) | 0x4C | I2C1 | FOUND |
| PCM1690 DAC2 (U2) | 0x4D | I2C1 | FOUND |
| PCM1690 DAC1 (U4) | 0x4E | I2C1 | FOUND |
| PCM1690 DAC3 (U8) | 0x4F | I2C1 | FOUND |
| EEPROM CAT24C256 | 0x50 | I2C4 | FOUND |

**Root cause discovered**: PCM1690 requires SCKI (MCLK) clock cycles after RST release
to complete internal POR. Without MCLK, I2C interface never enables.
**Fix**: bit-bang ~8000 clock cycles on PE2 before I2C scan. Integrated into main firmware.

**PowerPAD status**: NOT soldered on DS9 boards (confirmed by inspection). Chips still
initialize via I2C once MCLK is applied. Audio output to be verified.

## Detailed SAI Pin Errors

### Error 1: SAI1_A on PE7/PE8/PE9 — NO SAI FUNCTION
**pin_definitions.h lines 31-36:**
```
SAI1_FS_A_PIN  = PE7  → Datasheet: TIM1_ETR, DFSDM1, UART7_RX, QUADSPI, FMC — NO SAI
SAI1_SCK_A_PIN = PE8  → Datasheet: TIM1_CH1N, DFSDM1, UART7_TX, QUADSPI, FMC — NO SAI
SAI1_SD_A_PIN  = PE9  → Datasheet: TIM1_CH1, DFSDM1, UART7_RTS, QUADSPI, FMC — NO SAI
```
**Impact**: DAC0 (U3) receives NO audio data. Channels 0-7 dead.
**Root cause**: PE7/PE8/PE9 confused with PE4/PE5/PE6 which DO have SAI1 AF6.

### Error 2: SAI2_A labels swapped (PD12↔PD13)
**pin_definitions.h lines 51-56:**
```
SAI2_FS_A_PIN  = PD13  → Datasheet AF10: SAI2_SCK_A (not FS!)
SAI2_SCK_A_PIN = PD12  → Datasheet AF10: SAI2_FS_A (not SCK!)
SAI2_SD_A_PIN  = PD11  → Datasheet AF10: SAI2_SD_A ✓
```
**Impact**: SCK and FS labels swapped. STM32 AF10 on both pins is correct; only PCB
routing matters. Verify schematic: if PD13→U2_BCK and PD12→U2_LRCK, audio works.

### Error 3: SAI2_B on PD8/PD9/PD10 — WRONG PERIPHERAL
**pin_definitions.h lines 60-66:**
```
SAI2_FS_B_PIN  = PD10 → Datasheet AF6: SAI3_FS_B (not SAI2!)
SAI2_SCK_B_PIN = PD9  → Datasheet AF6: SAI3_SD_B (SCK and SD also swapped!)
SAI2_SD_B_PIN  = PD8  → Datasheet AF6: SAI3_SCK_B
```
**Actual AF6 mapping:**
- PD8 = SAI3_SCK_B
- PD9 = SAI3_SD_B
- PD10 = SAI3_FS_B

### Error 4: SAI4_B ADC pins — WRONG MAPPING
**pin_definitions.h lines 72-78:**
```
SAI4_FS_B_PIN  = PC0  → Datasheet: NO SAI4 function
SAI4_SCK_B_PIN = PC1  → Datasheet AF8: SAI4_SD_A (not SCK_B!)
SAI4_SD_B_PIN  = PB11 → Datasheet: NO SAI function at all
```
**Note**: SAI4_SCK_B=PF8 and SAI4_FS_B=PF9 are NOT on LQFP100.
SAI4_B can only work in synchronous mode on LQFP100.

### Error 5: PE11/PE12/PE13 pin conflicts
```
PE11 = DAC0_AMUTEI  → Conflicts with SAI2_SD_B (AF10)
PE12 = DAC0_AMUTEO  → Conflicts with SAI2_SCK_B (AF10)
PE13 = TP1          → Conflicts with SAI2_FS_B (AF10)
```
Correct SAI2_B pins (PE11/PE12/PE13) are occupied by DAC0 mute control and test point.

### Error 6: PE3 conflict
```
PE3 = DAC1_RST → Conflicts with SAI1_SD_B (AF6, needed for DAC1 in sync mode)
```
Cannot use SAI1_B for DAC1 without losing DAC1 reset control.

## Bodge Wire Options for DS9

### Option A: Get DAC3 (U8) working via SAI3_B — 24 channels
**Difficulty**: Medium (2 trace swaps on PD8↔PD9)
**Result**: DAC1(SAI1_A) + DAC2(SAI2_A) + DAC3(SAI3_B) = 24 channels

| DS9 PCB trace | Goes to DAC3 pin | Actual STM32 AF6 signal | Correct? |
|--------------|-----------------|----------------------|---------|
| PD8 (labeled SD) | U8 SD_A (data in) | SAI3_SCK_B | **WRONG** |
| PD9 (labeled SCK) | U8 BCK (clock) | SAI3_SD_B | **WRONG** |
| PD10 (labeled FS) | U8 LRCK (frame) | SAI3_FS_B | **OK** |

**Bodge**:
1. Cut trace PD8→U8 SD pin
2. Cut trace PD9→U8 BCK pin
3. Wire: PD8 → U8 BCK (STM32 SCK → DAC BCK ✓)
4. Wire: PD9 → U8 SD (STM32 SD → DAC SD ✓)

**Firmware**: Use SAI3_B Master TX with AF6. PD8/PD9/PD10 unchanged in firmware —
the bodge physically swaps where the signals land on the DAC.

### Option B: Get DAC0 (U3) — NOT RECOMMENDED
PE7/PE8/PE9 have zero SAI function. Would need 3 wires from PE4/PE5/PE6 area.
Too many traces to cut/reroute on 5 boards.

### Option C: ADC (U1 PCM1802) — HIGH DIFFICULTY
PC0, PC1, PB11 all wrong. Better to fix in DS10.

### Recommended DS9 Testing Strategy
1. **SAI1_A on PE4/PE5/PE6** → DAC1 (U4) — 8 channels (firmware ch 0-7)
2. **SAI2_A on PD11/PD12/PD13** → DAC2 (U2) — 8 channels (firmware ch 16-23)
3. **SAI3_B on PD8/PD9/PD10 with bodge** → DAC3 (U8) — 8 channels (firmware ch 24-31)
4. DAC0 (U3) — skip, fix in DS10
5. ADC (U1) — skip, fix in DS10

## Correct Pin Mapping for DS10

### SAI1 — DAC0 + DAC1 (Channels 0-15)
| Function | Pin | AF | Notes |
|----------|-----|-----|-------|
| SAI1_MCLK_A | PE2 | AF6 | Shared MCLK for all DACs |
| SAI1_FS_A | PE4 | AF6 | DAC0 LRCK |
| SAI1_SCK_A | PE5 | AF6 | DAC0 BCK |
| SAI1_SD_A | PE6 | AF6 | DAC0 data (ch 0-7) |
| SAI1_SD_B | PE3 | AF6 | DAC1 data (ch 8-15), sync mode |

### SAI2 — DAC2 + DAC3 (Channels 16-31)
| Function | Pin | AF | Notes |
|----------|-----|-----|-------|
| SAI2_FS_A | PD12 | AF10 | DAC2 LRCK (was labeled SCK in DS9!) |
| SAI2_SCK_A | PD13 | AF10 | DAC2 BCK (was labeled FS in DS9!) |
| SAI2_SD_A | PD11 | AF10 | DAC2 data (ch 16-23) |
| SAI2_SD_B | PE11 | AF10 | DAC3 data (ch 24-31), sync mode |

### SAI3 — ADC Input (RECOMMENDED for DS10)
| Function | Pin | AF | Notes |
|----------|-----|-----|-------|
| SAI3_SCK_B | PD8 | AF6 | ADC BCK |
| SAI3_SD_B | PD9 | AF6 | ADC DOUT |
| SAI3_FS_B | PD10 | AF6 | ADC LRCK |

### DS10 Pin Conflict Resolutions
| DS9 Pin | DS9 Function | DS10 Reassignment |
|---------|-------------|------------------|
| PE3 | DAC1_RST | Move RST to free GPIO (e.g., PC8, PC9) |
| PE7/PE8/PE9 | SAI1 (broken) | Free for AMUTEI/AMUTEO/other |
| PE11 | DAC0_AMUTEI | SAI2_SD_B for DAC3. Move AMUTEI to PE7 |
| PE12 | DAC0_AMUTEO | Free (sync mode doesn't need SAI2_SCK_B). Keep as AMUTEO |
| PE13 | TP1 | Free (sync mode doesn't need SAI2_FS_B) |
| PD8/9/10 | SAI2_B (broken) | SAI3_B for ADC |
| PC0/PC1/PB11 | SAI4 (broken) | Free GPIOs |

## DS10 Channel Assignment Summary
| SAI Block | Mode | Pins | DAC/ADC | Channels |
|-----------|------|------|---------|----------|
| SAI1_A | Master TX | PE4(FS) PE5(SCK) PE6(SD) | DAC0 (U3) | 0-7 |
| SAI1_B | Slave TX (sync) | PE3(SD only) | DAC1 (U4) | 8-15 |
| SAI2_A | Master TX | PD12(FS) PD13(SCK) PD11(SD) | DAC2 (U2) | 16-23 |
| SAI2_B | Slave TX (sync) | PE11(SD only) | DAC3 (U8) | 24-31 |
| SAI3_B | Master RX | PD8(SCK) PD9(SD) PD10(FS) | ADC (U1) | Input L+R |
| MCLK | Output | PE2 | All chips | 12.288 MHz |
| **Total** | | | **4 DAC + 1 ADC** | **32 out + 2 in** |
