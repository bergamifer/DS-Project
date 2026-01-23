# PCM1802 Hardware Configuration

**Note:** The PCM1802 is configured via hardware pins, not software registers. It uses GPIO pins for control.

## Overview
The PCM1802 consists of a reference circuit, two channels of single-ended-to-differential converter, a fifth-order delta-sigma modulator with full differential architecture, a decimation filter with high-pass filter, and a serial interface circuit.

- On-chip high-precision reference with one external capacitor.
- Full-differential architecture for wide dynamic range and excellent power-supply rejection.
- Fifth-order delta-sigma noise shaper.
- Supports 256 fS, 384 fS, 512 fS, 768 fS system clocks.
- Audio data formats: Left-justified, I2S, Right-justified (24-bit or 20-bit).

## Feature Description

### Hardware Control
The following pins control the device by tying to GPIO, GND, or VDD:
- FMT0, FMT1: Data format selection
- OSR: Oversampling ratio
- BYPASS: HPF bypass
- MD0, MD1: Interface mode
- PDWN: Power down

### Strapping Configuration
| Pin/Control | Connection          | Function                  | Note |
|-------------|---------------------|---------------------------|------|
| FMT0 (17)  | 3V3                 | I2S, 24-bit              | FMT1=0, FMT0=1 |
| FMT1 (18)  | GND                 | I2S, 24-bit              |      |
| MODE0 (19) | GND                 | Slave mode (256/384/512/768 fS) | MODE1=0, MODE0=0 |
| MODE1 (20) | GND                 | Slave mode                |      |
| PDWN       | STM32 PA1 (GPIO out)| Power down control        | ✓   |
| BYPASS     | STM32 PC3 (GPIO out)| HPF bypass control        | ✓   |

### Power-On Reset Sequence
- Internal reset when VDD > 2.2V (typical).
- Reset state for 1024 system-clock counts after power-on.
- Digital output valid after reset release and 4480 / fS time.

### System Clock
Supports 256 fS, 384 fS, 512 fS, 768 fS. Auto-detection in slave mode.

| Sampling Rate (kHz) | 256 fS (MHz) | 384 fS (MHz) | 512 fS (MHz) | 768 fS (MHz) |
|---------------------|--------------|--------------|--------------|--------------|
| 32                  | 8.192       | 12.288      | 16.384      | 24.576      |
| 44.1                | 11.2896     | 16.9344     | 22.5792     | 33.8688     |
| 48                  | 12.288      | 18.432      | 24.576      | 36.864      |
| 64                  | 16.384      | 24.576      | 32.768      | 49.152      |
| 88.2                | 22.5792     | 33.8688     | 45.1584     | —           |
| 96                  | 24.576      | 36.864      | 49.152      | —           |

## Device Functional Modes

### Power Down, HPF Bypass, Oversampling Control

| PDWN | Mode              |
|------|-------------------|
| LOW  | Power-down mode  |
| HIGH | Normal operation |

| BYPAS | HPF Mode                  |
|-------|---------------------------|
| LOW   | Normal (no DC on DOUT)   |
| HIGH  | Bypass (DC on DOUT)      |

| OSR  | Oversampling Ratio |
|------|--------------------|
| LOW  | ×64               |
| HIGH | ×128 (fS < 50 kHz)|

### Serial Audio Data Interface

#### Data Format
| Format | FMT1 | FMT0 | Description          |
|--------|------|------|----------------------|
| 0      | 0    | 0    | Left-justified, 24-bit |
| 1      | 0    | 1    | I2S, 24-bit          |
| 2      | 1    | 0    | Right-justified, 24-bit |
| 3      | 1    | 1    | Right-justified, 20-bit |

#### Audio Data Interface Timing: Slave Mode
| Parameter     | MIN | MAX | UNIT |
|---------------|-----|-----|------|
| t(BCKP)       | —   | 150 | ns  |
| t(BCKH)       | 60  | —   | ns  |
| t(BCKL)       | 60  | —   | ns  |
| t(LRSU)       | 40  | —   | ns  |
| t(LRHD)       | —   | 20  | ns  |
| t(LRCP)       | —   | 10  | μs  |
| t(FSSU)       | 20  | —   | ns  |
| t(FSHD)       | —   | 20  | ns  |
| t(CKDO)       | -10 | 20  | ns  |
| t(LRDO)       | -10 | 20  | ns  |
| tr            | —   | 10  | ns  |
| tf            | —   | 10  | ns  |

### Interface Mode
| MODE1 | MODE0 | Interface Mode          |
|-------|-------|-------------------------|
| 0     | 0     | Slave (256/384/512/768 fS) |
| 0     | 1     | Master (512 fS)         |
| 1     | 0     | Master (384 fS)         |
| 1     | 1     | Master (256 fS)         |

## Power Supply Recommendations
- VCC: 5V nominal (analog)
- VDD: 3.3V nominal (digital)
- Decoupling: 0.1μF ceramic + 10μF tantalum near pins.

## Layout Guidelines
- **VCC/VDD:** Bypass with 0.1μF + 10μF capacitors close to pins.
- **AGND/DGND:** Connect directly under the part.
- **VIN:** 1μF AC-coupling capacitor.
- **VREF1:** 0.1μF + 10μF capacitors.
- **VREF2:** 0.1μF + 10μF, optional 1kΩ resistor to VCC.
- **DOUT:** Minimize load capacitance; use buffer if needed.
- **System Clock:** Ensure low jitter and proper duty cycle.