# DS9 Beamforming -- STM32H743 Firmware

Firmware de audio beamforming: 5 SAI blocks (32 TX + 2 RX), DMA circular, delay lines por canal.

## Build & Flash

```bash
cd firmware
make -j4                 # Compilar
make flash-stlink        # Flash via ST-Link SWD
make clean && make -j4   # Clean build
```

Output: `build/ds11_beamforming.bin`

## Estructura

```
Core/
  Inc/
    pin_definitions.h       # Pines DS9 (LEDs, DAC RST/AMUTEI, etc.)
    stm32h7xx_hal_conf.h    # HAL module config
    uart_protocol.h         # Protocolo binario (ds_config_t)
    pcm1690.h               # PCM1690 DAC driver
    epaper.h                # SSD1681 e-paper driver
  Src/
    main.c                  # Init, clocks 480MHz, DMA, beamforming, main loop
    stm32h7xx_it.c          # IRQ handlers (DMA, SAI, UART4) + HardFault dump
    stm32h7xx_hal_msp.c     # MSP init (SAI, UART, I2C, SPI)
    uart_protocol.c         # Parser binario ESP32->STM32
    pcm1690.c               # PCM1690 DAC I2C driver
    epaper.c                # SSD1681 driver
STM32H743XX_FLASH.ld       # Linker script (.dma_buffer -> RAM_D2)
Makefile                    # TARGET=ds11_beamforming
```

## Clocks

| Clock | Freq | Uso |
|-------|------|-----|
| SYSCLK (PLL1) | 480 MHz | CPU (VOS0) |
| HCLK | 240 MHz | AHB buses |
| APB1-4 | 120 MHz | Perifericos |
| PLL3P | 12.288 MHz | SAI MCLK (256 x 48kHz) |

## Perifericos

| Periferico | Pines | Funcion |
|------------|-------|---------|
| SAI1_A+B | PE2-6, PE3 | DAC0+DAC1 (ch 0-15) |
| SAI2_A+B | PD11-13, PE11 | DAC2+DAC3 (ch 16-31) |
| SAI3_B | PD8-10 | ADC RX (2ch) |
| UART4 | PA0(TX), PA1(RX) | ESP32 link (115200 8N1) |
| UART7 | PA8(RX), PA15(TX) | Debug serial (STLink) |
| I2C1 | PB8(SCL), PB9(SDA) | 4x PCM1690 DAC config |
| SPI2 | PB13(SCK), PB15(MOSI) | E-Paper display |

## Beamforming Pipeline

```
SAI3_B RX DMA -> audio_buffer_3b_rx (interleaved L,R,L,R...)
                       | Process_ADC_Half()
                 delay_line_L[1024] / delay_line_R[1024]
                       | Fill_Audio_Beamform(buf, samples, ch_offset)
    Per channel: read delay_line[write_pos - delay] * gain >> 8 * master >> 8, mute
                       |
    SAI1_A(ch0-7) / SAI1_B(ch8-15) / SAI2_A(ch16-23) / SAI2_B(ch24-31)
```

## Notas Tecnicas

- **DMA buffers en RAM_D2** (0x30000000): DMA1/DMA2 NO acceden DTCMRAM
- **D-cache deshabilitado**: usar `__DSB()` como memory barrier (NO `SCB_CleanDCache_by_Addr`)
- **SysTick puede estar starved**: SAI/I2C ISRs en priority 0, SysTick en priority 15. Usar `DWT->CYCCNT` para timing critico
- **DS9 pin conflicts**: PE3 (DAC1_RST / SAI1_SD_B), PE11 (DAC0_AMUTEI / SAI2_SD_B) -- init DAC primero, luego reconfigurar AF
