# PCM1690 Quick Reference

## I2C Address Configuration
| AD1 | AD0 | Address (Write) | Address (Read) |
|-----|-----|-----------------|----------------|
| 0   | 0   | 0x98           | 0x99          |
| 0   | 1   | 0x9A           | 0x9B          |
| 1   | 0   | 0x9C           | 0x9D          |
| 1   | 1   | 0x9E           | 0x9F          |

**Init Sequence**:
1. Write 0x40 = 0x80 (Reset)
2. Wait 10ms
3. Write 0x40 = 0x00 (TDM mode, unmute)
4. Write 0x41-0x48 = 0xFF (max volume todas las salidas)

## Pin Configuration
- SCKI: 256fs o 384fs (típico 256fs = 12.288 MHz @ 48kHz)
- BCK: 64fs (3.072 MHz @ 48kHz)
- LRCK: fs (48 kHz)
- DIN: MSB first, cambio en falling edge de BCK

## Power Supply Pins
- AVDD: +5V (analógico)
- DVDD: +5V (core DAC)
- VDD: +3.3V (digital I/O e I2C)
- AGND, DGND: separados hasta pad térmico

## Pin Functions
| PIN | I/O | PULLDOWN | 5-V TOLERANT | DESCRIPTION | NAME |
|-----|-----|----------|---------------|-------------|------|
| 1   | —  | —        | —             | Reserved, tied to analog ground | RSV2 |
| 2   | —  | —        | —             | Reserved, left open | RSV1 |
| 3   | —  | —        | —             | Reserved, tied to analog ground | RSV2 |
| 4   | —  | —        | —             | Reserved, left open | RSV1 |
| 5   | —  | —        | —             | Reserved, tied to analog ground | RSV2 |
| 6   | I  | Yes      | No            | Audio data word clock input | LRCK |
| 7   | I  | Yes      | No            | Audio data bit clock input | BCK |
| 8   | I  | No       | No            | Audio data input for DAC1 and DAC2 | DIN1 |
| 9   | I  | No       | No            | Audio data input for DAC3 and DAC4 | DIN2 |
| 10  | I  | No       | No            | Audio data input for DAC5 and DAC6 | DIN3 |

## Overview
The PCM1690 is a high-performance, multi-channel DAC for consumer audio applications (Blu-ray DVD, HD DVD, home theaters, A/V receivers). It features an eight-channel DAC with differential output configuration. Supports 16-/20-/24-/32-bit linear PCM in I2S and left-justified formats, and 24-bit in right-justified, DSP, and TDM formats, for sampling frequencies 8-192 kHz. TDM format saves bus lines for multi-channel communication. Control modes: I2C, SPI, or hardware.

- Audio data interface formats: I2S, LJ, RJ, DSP, TDM
- Audio data word length: 16, 20, 24, 32 Bits
- Audio data format: MSB first, twos complement

## System Clock Timing Requirements
| Parameter | MIN | MAX | UNIT |
|-----------|-----|-----|------|
| tSCY (System clock cycle time) | — | 27 | ns |
| tSCH (System clock width high) | 10 | — | ns |
| tSCL (System clock width low) | 10 | — | ns |
| System clock duty cycle | 40% | 60% | — |

## Audio Interface Timing for I2S/LJ/RJ Formats
| Parameter | MIN | MAX | UNIT |
|-----------|-----|-----|------|
| tBCY (BCK cycle time) | — | 75 | ns |
| tBCH (BCK pulse width high) | 35 | — | ns |
| tBCL (BCK pulse width low) | 35 | — | ns |
| tLRS (LRCK set-up to BCK rising) | 10 | — | ns |
| tLRH (LRCK hold to BCK rising) | 10 | — | ns |
| tDIS (DIN set-up to BCK rising) | 10 | — | ns |
| tDIH (DIN hold to BCK rising) | 10 | — | ns |

## Audio Interface Timing for DSP/TDM Formats
| Parameter | MIN | MAX | UNIT |
|-----------|-----|-----|------|
| tBCY (BCK cycle time) | — | 40 | ns |
| tBCH (BCK pulse width high) | 15 | — | ns |
| tBCL (BCK pulse width low) | 15 | — | ns |
| tLRW (LRCK pulse width high DSP) | tBCY | tBCY | — |
| tLRW (LRCK pulse width high TDM) | tBCY | 1/fS – tBCY | — |
| tLRS (LRCK set-up to BCK rising) | 10 | — | ns |
| tLRH (LRCK hold to BCK rising) | 10 | — | ns |
| tDIS (DIN set-up to BCK rising) | 10 | — | ns |
| tDIH (DIN hold to BCK rising) | 10 | — | ns |