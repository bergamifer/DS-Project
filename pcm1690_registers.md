# PCM1690 Register Map

## Pin Descriptions
MODE        : selecciona SPI / I2C / HW
MD/SDA      : MOSI (SPI) o SDA (I2C)
MC/SCL      : SCLK (SPI) o SCL (I2C)
MS/ADR0     : CS (SPI) o A0 (I2C)
TEST/ADR1   : A1 (I2C)

## User-Programmable Mode Control Functions
| FUNCTION | RESET DEFAULT | REGISTER | LABEL |
|----------|----------------|----------|-------|
| Mode control register reset | Normal operation | 64 | MRST |
| System reset | Normal operation | 64 | SRST |
| Analog mute function control | Mute disabled | 64 | AMUTE[3:0] |
| Sampling mode selection | Auto | 64 | SRDA[1:0] |
| Power-save mode selection | Power save | 65 | PSMDA |
| Audio interface format selection | I2S | 65 | FMTDA[3:0] |
| Operation control | Normal operation | 66 | OPEDA[3:0] |
| Digital filter roll-off control | Sharp roll-off | 66 | FLT[3:0] |
| Output phase selection | Normal | 67 | REVDA[8:1] |
| Soft mute control | Mute disabled | 68 | MUTDA[8:1] |
| Zero flag | Not detected | 69 | ZERO[8:1] |
| Digital attenuation mode | 0 dB to –63 dB, 0.5 dB step | 70 | DAMS |
| Digital de-emphasis function control | Disabled | 70 | DEMP[1:0] |
| Zero flag function selection | ZERO1: DIN1, left-channel | 70 | AZRO[1:0] |
| Zero flag polarity selection | High for detection | 70 | ZREV |
| Digital attenuation level setting | 0 dB, no attenuation | 71–79 | ATDAx[7:0] |

## Register Map
| ADR[6:0] | DEC | HEX | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
|----------|-----|-----|----|----|----|----|----|----|----|----|
| 64 | 64 | 40 | MRST | SRST | AMUTE3 | AMUTE2 | AMUTE1 | AMUTE0 | SRDA1 | SRDA0 |
| 65 | 65 | 41 | PSMDA | RSV(1) | RSV(1) | RSV(1) | FMTDA3 | FMTDA2 | FMTDA1 | FMTDA0 |
| 66 | 66 | 42 | OPEDA3 | OPEDA2 | OPEDA1 | OPEDA0 | FLT3 | FLT2 | FLT1 | FLT0 |
| 67 | 67 | 43 | REVDA8 | REVDA7 | REVDA6 | REVDA5 | REVDA4 | REVDA3 | REVDA2 | REVDA1 |
| 68 | 68 | 44 | MUTDA8 | MUTDA7 | MUTDA6 | MUTDA5 | MUTDA4 | MUTDA3 | MUTDA2 | MUTDA1 |
| 69 | 69 | 45 | ZERO8 | ZERO7 | ZERO6 | ZERO5 | ZERO4 | ZERO3 | ZERO2 | ZERO1 |
| 70 | 70 | 46 | DAMS | RSV(1) | DEMP1 | DEMP0 | RSV(1) | AZRO1 | AZRO0 | ZREV |
| 71 | 71 | 47 | RSV(1) | RSV(1) | RSV(1) | RSV(1) | RSV(1) | RSV(1) | RSV(1) | RSV(1) |
| 72 | 72 | 48 | ATDA17 | ATDA16 | ATDA15 | ATDA14 | ATDA13 | ATDA12 | ATDA11 | ATDA10 |
| 73 | 73 | 49 | ATDA27 | ATDA26 | ATDA25 | ATDA24 | ATDA23 | ATDA22 | ATDA21 | ATDA20 |
| 74 | 74 | 4A | ATDA37 | ATDA36 | ATDA35 | ATDA34 | ATDA33 | ATDA32 | ATDA31 | ATDA30 |
| 75 | 75 | 4B | ATDA47 | ATDA46 | ATDA45 | ATDA44 | ATDA43 | ATDA42 | ATDA41 | ATDA40 |
| 76 | 76 | 4C | ATDA57 | ATDA56 | ATDA55 | ATDA54 | ATDA53 | ATDA52 | ATDA51 | ATDA50 |
| 77 | 77 | 4D | ATDA67 | ATDA66 | ATDA65 | ATDA64 | ATDA63 | ATDA62 | ATDA61 | ATDA60 |
| 78 | 78 | 4E | ATDA77 | ATDA76 | ATDA75 | ATDA74 | ATDA73 | ATDA72 | ATDA71 | ATDA70 |
| 79 | 79 | 4F | ATDA87 | ATDA86 | ATDA85 | ATDA84 | ATDA83 | ATDA82 | ATDA81 | ATDA80 |

## Register Definitions

### Register 64 (Hex 40)
| B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
|----|----|----|----|----|----|----|----|
| MRST | SRST | AMUTE3 | AMUTE2 | AMUTE1 | AMUTE0 | SRDA1 | SRDA0 |

- **MRST**: Mode control register reset (0: Set default, 1: Normal operation, default=1)
- **SRST**: System reset (0: Resynchronization, 1: Normal operation, default=1)
- **AMUTE[3:0]**: Analog mute function control (default=0000)
  - xxx0: Disable by SCKI halt
  - xxx1: Enable by SCKI halt
  - xx0x: Disable by asynchronous detect
  - xx1x: Enable by asynchronous detect
  - x0xx: Disable by ZERO1 and ZERO2 detect
  - x1xx: Enable by ZERO1 and ZERO2 detect
  - 0xxx: Disable by DAC disable command
  - 1xxx: Enable by DAC disable command
- **SRDA[1:0]**: Sampling mode selection (00: Auto, 01: Single rate, 10: Dual rate, 11: Quad rate, default=00)

### Register 65 (Hex 41)
| B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
|----|----|----|----|----|----|----|----|
| PSMDA | RSV | RSV | RSV | FMTDA3 | FMTDA2 | FMTDA1 | FMTDA0 |

- **PSMDA**: Power-save mode selection (0: Power-save enable, 1: Power-save disable, default=0)
- **RSV**: Reserved
- **FMTDA[3:0]**: Audio interface format selection (default=0000)
  - 0000: 16-/20-/24-/32-bit I2S
  - 0001: 16-/20-/24-/32-bit left-justified
  - 0010: 24-bit right-justified
  - 0011: 16-bit right-justified
  - 0100: 24-bit I2S DSP
  - 0101: 24-bit left-justified DSP
  - 0110: 24-bit I2S TDM
  - 0111: 24-bit left-justified TDM
  - 1000: 24-bit high-speed I2S TDM
  - 1001: 24-bit high-speed left-justified TDM
  - 101x-11xx: Reserved

### Register 66 (Hex 42)
| B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
|----|----|----|----|----|----|----|----|
| OPEDA3 | OPEDA2 | OPEDA1 | OPEDA0 | FLT3 | FLT2 | FLT1 | FLT0 |

- **OPEDA[3:0]**: Operation control (default=0000)
  - xxx0: DAC1/2 normal
  - xxx1: DAC1/2 disable
  - xx0x: DAC3/4 normal
  - xx1x: DAC3/4 disable
  - x0xx: DAC5/6 normal
  - x1xx: DAC5/6 disable
  - 0xxx: DAC7/8 normal
  - 1xxx: DAC7/8 disable
- **FLT[3:0]**: Digital filter roll-off (default=0000)
  - xxx0: DAC1/2 sharp
  - xxx1: DAC1/2 slow
  - xx0x: DAC3/4 sharp
  - xx1x: DAC3/4 slow
  - x0xx: DAC5/6 sharp
  - x1xx: DAC5/6 slow
  - 0xxx: DAC7/8 sharp
  - 1xxx: DAC7/8 slow

### Register 67 (Hex 43)
| B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
|----|----|----|----|----|----|----|----|
| REVDA8 | REVDA7 | REVDA6 | REVDA5 | REVDA4 | REVDA3 | REVDA2 | REVDA1 |

- **REVDA[8:1]**: Output phase selection (default=00000000)
  - xxxx xxx0: DAC1 normal
  - xxxx xxx1: DAC1 inverted
  - xxxx xx0x: DAC2 normal
  - xxxx xx1x: DAC2 inverted
  - xxxx x0xx: DAC3 normal
  - xxxx x1xx: DAC3 inverted
  - xxxx 0xxx: DAC4 normal
  - xxxx 1xxx: DAC4 inverted
  - xxx0 xxxx: DAC5 normal
  - xxx1 xxxx: DAC5 inverted
  - xx0x xxxx: DAC6 normal
  - xx1x xxxx: DAC6 inverted
  - x0xx xxxx: DAC7 normal
  - x1xx xxxx: DAC7 inverted
  - 0xxx xxxx: DAC8 normal
  - 1xxx xxxx: DAC8 inverted

### Register 68 (Hex 44)
| B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
|----|----|----|----|----|----|----|----|
| MUTDA8 | MUTDA7 | MUTDA6 | MUTDA5 | MUTDA4 | MUTDA3 | MUTDA2 | MUTDA1 |

- **MUTDA[8:1]**: Soft mute control (default=00000000)
  - xxxx xxx0: DAC1 mute disabled
  - xxxx xxx1: DAC1 mute enabled
  - xxxx xx0x: DAC2 mute disabled
  - xxxx xx1x: DAC2 mute enabled
  - xxxx x0xx: DAC3 mute disabled
  - xxxx x1xx: DAC3 mute enabled
  - xxxx 0xxx: DAC4 mute disabled
  - xxxx 1xxx: DAC4 mute enabled
  - xxx0 xxxx: DAC5 mute disabled
  - xxx1 xxxx: DAC5 mute enabled
  - xx0x xxxx: DAC6 mute disabled
  - xx1x xxxx: DAC6 mute enabled
  - x0xx xxxx: DAC7 mute disabled
  - x1xx xxxx: DAC7 mute enabled
  - 0xxx xxxx: DAC8 mute disabled
  - 1xxx xxxx: DAC8 mute enabled

### Register 69 (Hex 45)
| B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
|----|----|----|----|----|----|----|----|
| ZERO8 | ZERO7 | ZERO6 | ZERO5 | ZERO4 | ZERO3 | ZERO2 | ZERO1 |

- **ZERO[8:1]**: Zero flag (read-only)
  - xxxx xxx0: DAC1 zero not detected
  - xxxx xxx1: DAC1 zero detected
  - xxxx xx0x: DAC2 zero not detected
  - xxxx xx1x: DAC2 zero detected
  - xxxx x0xx: DAC3 zero not detected
  - xxxx x1xx: DAC3 zero detected
  - xxxx 0xxx: DAC4 zero not detected
  - xxxx 1xxx: DAC4 zero detected
  - xxx0 xxxx: DAC5 zero not detected
  - xxx1 xxxx: DAC5 zero detected
  - xx0x xxxx: DAC6 zero not detected
  - xx1x xxxx: DAC6 zero detected
  - x0xx xxxx: DAC7 zero not detected
  - x1xx xxxx: DAC7 zero detected
  - 0xxx xxxx: DAC8 zero not detected
  - 1xxx xxxx: DAC8 zero detected

### Register 70 (Hex 46)
| B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
|----|----|----|----|----|----|----|----|
| DAMS | RSV | DEMP1 | DEMP0 | RSV | AZRO1 | AZRO0 | ZREV |

- **DAMS**: Digital attenuation mode (0: 0.5dB step 0-63dB, 1: 1dB step 0-100dB, default=0)
- **RSV**: Reserved
- **DEMP[1:0]**: Digital de-emphasis (00: Disable, 01: 48kHz, 10: 44.1kHz, 11: 32kHz, default=00)
- **AZRO[1:0]**: Zero flag combination (00: A, 01: B, 10: C, 11: D, default=00)
- **ZREV**: Zero flag polarity (0: High for detect, 1: Low for detect, default=0)

### Registers 71-79 (Hex 47-4F) - Digital Attenuation Levels
| Register | HEX | Bits |
|----------|-----|------|
| 71 | 47 | RSV x8 |
| 72 | 48 | ATDA1[7:0] |
| 73 | 49 | ATDA2[7:0] |
| 74 | 4A | ATDA3[7:0] |
| 75 | 4B | ATDA4[7:0] |
| 76 | 4C | ATDA5[7:0] |
| 77 | 4D | ATDA6[7:0] |
| 78 | 4E | ATDA7[7:0] |
| 79 | 4F | ATDA8[7:0] |

- **ATDAx[7:0]**: Attenuation level for DAC x (0-255, attenuation = step * (value - 255), default=255=0dB)
  - For DAMS=0: 0.5dB steps, mute at 0-128
  - For DAMS=1: 1dB steps, mute at 0-154
- **RSV**: Reserved

### Attenuation Levels Table
| ATDAx[7:0] | Decimal | DAMS=0 | DAMS=1 |
|------------|---------|--------|--------|
| 11111111 | 255 | 0 dB | 0 dB |
| 11111110 | 254 | -0.5 dB | -1 dB |
| ... | ... | ... | ... |
| 10011011 | 155 | -50.0 dB | -100 dB |
| 10011010 | 154 | -50.5 dB | Mute |
| ... | ... | ... | ... |
| 00000000 | 0 | Mute | Mute |