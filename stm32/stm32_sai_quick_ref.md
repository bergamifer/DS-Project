# STM32H743 SAI - Quick Reference

## Clock Configuration
**Source**: PLL2 o PLL3 (recomendado PLL3 para audio)
**Formula**: SAI_Clock = PLL_Clock / MCKDIV

Para 48kHz:
- MCLK = 12.288 MHz (256 × 48kHz)
- BCK = 3.072 MHz (64 × 48kHz)
- LRCK = 48 kHz

**Ejemplo config PLL3**:
- PLL3_VCO = 344.064 MHz
- PLL3_P = 28 → 12.288 MHz para SAI MCLK

## SAI Pins Alternative Functions
| Pin | AF6 | AF8 | AF10 |
|-----|-----|-----|------|
| PE2 | SAI1_MCLK_A | SAI4_MCLK_A | - |
| PE4 | SAI1_FS_A | SAI4_FS_A | - |
| PE5 | SAI1_SCK_A | SAI4_SCK_A | - |
| PE6 | SAI1_SD_A | SAI4_SD_A | - |
| PC0 | - | - | SAI4_FS_B |
| PC1 | - | - | SAI4_SCK_B |
| PB11| - | - | SAI4_SD_B |

## Register Setup (modo TDM 8 canales)
**SAI_xCR1**:
- MODE = 00 (Master transmitter)
- DS = 100 (24 bits)
- MCKDIV = valor calculado según PLL

**SAI_xFRCR**:
- FRL = 255 (Frame length = 256 bits para TDM8)
- FSALL = 0 (pulse width)

**SAI_xSLOTR**:
- NBSLOT = 7 (8 slots: 0-7)
- SLOTEN = 0xFF (todos los slots activos)