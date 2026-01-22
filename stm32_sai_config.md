# SAI Configuration - STM32H743

## Registro importante: SAI_xCR1
- Bit 5-4: DS[2:0] - Data size
  - 010: 16 bits
  - 100: 24 bits
  
## Clock Configuration
- SAI clock source: PLL
- Typical: 98.304 MHz para 48kHz
...