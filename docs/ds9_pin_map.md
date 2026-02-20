# DS9 Board Pin Map — STM32H743VI LQFP100

> **Status**: 5 boards fabricated (JLCPCB). 6 SAI pin errors found vs DS10.
> See [ds9_pin_audit.md](ds9_pin_audit.md) for SAI errors. This file covers all confirmed DS9 connections.

---

## I2C Buses

| Bus  | SCL  | SDA  | AF  | Connected to |
|------|------|------|-----|--------------|
| I2C1 | PB8  | PB9  | AF4 | All 4x PCM1690 DACs (pull-ups: RN1 4.7kΩ) |
| I2C4 | PB6  | PB7  | AF6 | EEPROM CAT24C256 U6 (pull-ups on PCB) |

---

## PCM1690 DAC Chips (HTSSOP-48)

| Signal      | DAC0 (U3) | DAC1 (U4) | DAC2 (U2) | DAC3 (U8) |
|-------------|-----------|-----------|-----------|-----------|
| I2C 7-bit   | 0x4C      | 0x4E      | 0x4D      | 0x4F      |
| ADR1 (p23)  | GNDA (0)  | +3V3 (1)  | GNDA (0)  | +3V3 (1)  |
| ADR0 (p22)  | GNDA (0)  | GNDA (0)  | +3V3 (1)  | +3V3 (1)  |
| MODE (p24)  | GNDA via R11 | GNDA via R15 | GNDA via R7 | GNDA via R41 |
| **RST**     | **PE10**  | **PE3**   | **PC10**  | **PB15**  |
| **AMUTEI**  | **PE11**  | **PE1**   | **PC11**  | **PB14**  |
| **AMUTEO**  | **PE12**  | **PE0**   | **PC12**  | **PB13**  |
| VDD (dig)   | 3.3V      | 3.3V      | 3.3V      | 3.3V      |
| VCC1/VCC2   | 5VA       | 5VA       | 5VA       | 5VA       |
| AGND1 (p27) | GND plane | GND plane | GND plane | GND plane |
| AGND2 (p46) | GND plane | GND plane | GND plane | GND plane |
| PowerPAD    | AGND ⚠️   | AGND ⚠️   | AGND ⚠️   | AGND ⚠️   |

> ⚠️ **PowerPAD NOT soldered on DS9 boards** — chips may not initialize.
> AGND1 (p27) and AGND2 (p46) are edge pins that should be soldered normally.

### Pin Behavior
- **RST**: Active LOW. Sequence: LOW 10ms → HIGH → 50ms delay → chip ready.
- **AMUTEI**: Input to chip. HIGH = normal, LOW = hardware mute. Firmware drives HIGH.
- **AMUTEO**: Output from chip. LOW = chip auto-muted (no MCLK / undervoltage).
  Reading this pin after RST is a quick diagnostic: stays L if chip is not running.
- **MODE**: All 4 chips tied to GNDA → I2C mode. Requires GNDA connected.

---

## EEPROM

| Signal   | Value |
|----------|-------|
| Part     | CAT24C256 (U6), 32KB |
| I2C addr | 0x50 (all address pins to GND) |
| Bus      | I2C4 — PB6 (SCL AF6), PB7 (SDA AF6) |

---

## UART Debug

| Signal | Pin | AF  | Notes |
|--------|-----|-----|-------|
| TX     | PA0 | AF8 | UART4, 115200 8N1 |
| RX     | PA1 | AF8 | UART4 |

Connect USB-UART adapter: PA0→RX of adapter, PA1→TX of adapter.

---

## Analog Rail Enable

| Pin  | Direction | Active | Effect |
|------|-----------|--------|--------|
| PA10 | Output    | HIGH   | Enables 3.3VA analog rail |

---

## LEDs

| LED | Pin | Active | Use in i2c_scan firmware |
|-----|-----|--------|--------------------------|
| D6  | PC4 | HIGH   | ON during I2C scan |
| D7  | PC5 | HIGH   | Blink once per found device |
| D11 | PB0 | HIGH   | ON if any PCM1690 found |
| D10 | PB1 | HIGH   | ON if EEPROM found |

---

## SAI — Audio (⚠️ DS9 has pin errors vs DS10)

> See [ds9_pin_audit.md](ds9_pin_audit.md) for full SAI error analysis.

| SAI Block | FS   | SCK  | SD   | DAC  | DS9 Status |
|-----------|------|------|------|------|------------|
| SAI1_A    | PE4  | PE5  | PE6  | DAC0 (U3) | ❌ SAI pin error |
| SAI2_A    | PD12 | PD13 | PD11 | DAC2 (U2) | ✅ Working |
| SAI1_B    | sync | sync | PE3  | DAC1 (U4) | ✅ OK — PE3 shared with DAC1/RST! |
| SAI2_B    | sync | sync | PE11 | DAC3 (U8) | ❌ PE11 = AMUTEI_0 conflict |
| SAI3_B RX | PD8  | PD9  | PD10 | ADC input | ✅ Working |

**PE3 conflict on DS9**: PE3 = DAC1/RST in DS9. PE3 = SAI1_SD_B in DS10.
For SAI audio test: release RST (PE3 HIGH) first, then reconfigure PE3 as SAI AF.

**PE11 conflict on DS9**: PE11 = AMUTEI_0. PE11 = SAI2_SD_B in DS10.
SAI2_B on DS9 requires PE11 as SAI AF — cannot drive AMUTEI_0 simultaneously.

---

## SAI MCLK

| Pin | AF  | Feeds |
|-----|-----|-------|
| PE2 | AF6 | All PCM1690 chips (12.288 MHz) |

---

## SPI — E-Paper (SSD1681)

| Signal | Pin  |
|--------|------|
| CS     | PB12 |
| DC     | PD4  |
| RST    | PD5  |
| BUSY   | PD6  |

---

## Clock Configuration

| Firmware        | Config |
|-----------------|--------|
| i2c_scan        | HSI 64MHz, no PLL, FLASH_LATENCY_1 |
| main (beamform) | HSE 25MHz → PLL1 480MHz, HCLK 240MHz |
| PLL3 MCLK       | 12.288MHz (DIVM3=5, DIVN3=68, DIVP3=28, FRACN=6660) |

---

## Software Diagnostic Results (2026-02-18)

**RESOLVED (2026-02-18)**: PCM1690 requires SCKI (MCLK) clock cycles after RST
release to complete internal POR. Without MCLK, I2C never enables.
Fix: bit-bang ~8000 clock cycles on PE2 before I2C scan. All 4 DACs found at
0x4C, 0x4D, 0x4E, 0x4F.

| Test | Result | Meaning |
|------|--------|---------|
| EEPROM I2C4 (0x50) | FOUND | STM32 I2C works, pull-ups OK, +3V3 OK |
| AMUTEO +3V3 probe (pulldown) | All H | +3V3 reaches all 4 chip areas |
| RST readback | All H | RST GPIOs driving correctly |
| AMUTEI readback | All H | AMUTEI GPIOs driving correctly |
| PA10 inverted | No change | PA10 not used in DS9 power design |
| I2C at 10kHz | No change | Not a signal integrity issue |
| RST=LOW scan | TIMEOUT=normal | Chips not blocking bus |
| SCL/SDA idle | H/H | Bus electrically clean |
| SDA/SCL continuity | Verified | Traces reach STM32→PCM1690 pins 20/21 |
| FB2 ferrite bead | Populated | +5VA path exists |
| **PowerPAD** | **NOT soldered** | **Confirmed by user** |

---

## Multimeter Checklist (pendiente)

Measure at the **PCM1690 chip body edge pins** (any of U2/U3/U4/U8).

### Quick triage (3 measurements, start here)

| # | What | Where | Expected | If wrong |
|---|------|-------|----------|----------|
| 1 | **VCC1 voltage** | Pin 25 vs GND | ~5V | +5VA not reaching chip → POR stuck |
| 2 | **MODE voltage** | Pin 24 vs GND | ~0V | >1V → chip in SPI mode, ignores I2C |
| 3 | **VDD voltage** | Pin 12 vs GND | 3.3V | No digital power |

### Full checklist (if triage passes)

| # | What | Where | Expected | If wrong |
|---|------|-------|----------|----------|
| 4 | VCC2 voltage | Pin 47 vs GND | ~5V | +5VA rail problem |
| 5 | DGND continuity | Pin 13 to board GND | <1Ω | Digital ground open |
| 6 | AGND1 continuity | Pin 27 to board GND | <1Ω | Analog ground open |
| 7 | AGND2 continuity | Pin 46 to board GND | <1Ω | Analog ground open |
| 8 | SCL idle voltage | Pin 21 vs GND | 3.3V | I2C clock not at chip |
| 9 | SDA idle voltage | Pin 20 vs GND | 3.3V | I2C data not at chip |
| 10 | RST voltage | Pin 15 vs GND | 3.3V (after boot) | RST trace open |
| 11 | PowerPAD cont. | Pad under chip to GND | open (expected) | — |

### Interpretation

```
VDD=3.3V + VCC1=5V + DGND OK + AGND OK + MODE=0V + SCL/SDA=3.3V + RST=3.3V
  → Everything correct → PowerPAD is the sole cause
  → Fix: reflow chips with hot air to solder PowerPAD

VCC1=0V (most likely failure)
  → +5VA not reaching chip → POR incomplete → no I2C
  → Check: FB2 output voltage, +5V rail at test point TP6

MODE > 1V
  → Chip in SPI mode → ignores all I2C
  → Check: R7/R11/R15/R41 solder quality

VDD=0V
  → Cold solder joint on pin 12
  → Reflow/touch up

SCL or SDA ≠ 3.3V
  → I2C trace problem (unlikely — continuity already verified)
```
