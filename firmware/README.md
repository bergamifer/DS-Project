# WeAct STM32H743 - Beamforming Audio Firmware

Firmware de audio beamforming con 5 SAI blocks (32 TX + 2 RX) y delay lines por canal.
Placa WeAct configurada con pinout DS10.

## Estado: Beamforming Pipeline Funcionando

- CPU 480MHz (VOS0), 5 SAI DMA, 0 errores, 0 underruns
- ADC RX (SAI3_B) → circular delay lines (L/R) → per-channel delay/gain/mute → 32ch DAC TX
- UART4: ESP32 binary config → beam_config_t (32 canales)
- E-Paper display (SSD1681, opcional)
- Latency/jitter characterization (DWT CYCCNT)

## Estructura

```
firmware/
├── Core/
│   ├── Inc/
│   │   ├── stm32h7xx_hal_conf.h   # HAL module config
│   │   ├── stm32h7xx_it.h         # Interrupt prototypes
│   │   ├── pin_definitions_ds10.h  # DS10 corrected pin assignments
│   │   ├── epaper.h               # SSD1681 e-paper driver
│   │   ├── epaper_fonts.h         # Font data 8x6 y 16x8
│   │   ├── pcm1690.h              # PCM1690 DAC driver
│   │   └── uart_protocol.h        # Binary UART protocol (ds_config_t)
│   └── Src/
│       ├── main.c                 # Beamforming pipeline, DMA callbacks, main loop
│       ├── stm32h7xx_hal_msp.c    # MSP init (5x SAI, UART4, UART7, I2C1, SPI2)
│       ├── stm32h7xx_it.c         # IRQ handlers (5x DMA, SAI1, SAI3, UART4)
│       ├── epaper.c               # SSD1681 driver
│       ├── pcm1690.c              # PCM1690 DAC driver
│       └── uart_protocol.c        # ESP32 binary protocol parser
├── Drivers/                       # STM32 HAL drivers
├── STM32H743XX_FLASH.ld           # Linker script (.dma_buffer → RAM_D2)
└── Makefile                       # Build system
```

## Build & Flash

```bash
make -j4                 # Compilar
make flash-stlink        # Flash via ST-Link SWD
make clean && make -j4   # Clean build
```

## Clocks

| Clock | Freq | Uso |
|-------|------|-----|
| SYSCLK (PLL1) | 480 MHz | CPU (VOS0 overdrive) |
| HCLK | 240 MHz | AHB buses |
| APB1-4 | 120 MHz | Peripherals |
| PLL3P | 12.288 MHz | SAI1/2/3 MCLK (256 x 48kHz) |

## SAI Configuration (DS10 Pinout)

| SAI Block | Mode | Pins | AF | DMA Stream | Req | Channels |
|-----------|------|------|----|------------|-----|----------|
| SAI1_A | Master TX TDM8 | PE4(FS) PE5(SCK) PE6(SD) | AF6 | DMA1_Stream0 | 87 | 0-7 |
| SAI1_B | Slave TX sync | PE3(SD only) | AF6 | DMA1_Stream3 | 88 | 8-15 |
| SAI2_A | Master TX TDM8 | PD12(FS) PD13(SCK) PD11(SD) | AF10 | DMA1_Stream1 | 89 | 16-23 |
| SAI2_B | Slave TX sync | PE11(SD only) | AF10 | DMA1_Stream2 | 90 | 24-31 |
| SAI3_B | Master RX I2S | PD8(SCK) PD9(SD) PD10(FS) | AF6 | DMA1_Stream4 | 114 | ADC L+R |

## Other Peripherals

| Peripheral | Pins | Function |
|------------|------|----------|
| UART7 | PA8(RX), PA15(TX) | Debug serial (minicom) |
| UART4 | PA0(TX), PA1(RX) | ESP32 protocol (115200 8N1) |
| SPI2 | PB13(SCK), PB15(MOSI) | E-Paper display |
| I2C1 | PB8(SCL), PB9(SDA) | DAC config (PCM1690) |
| E-Paper | PB12(CS), PD4(DC), PD5(RST), PD6(BUSY) | SSD1681 display |
| GPIO | PB5 | Scope trigger (fill timing) |

## Beamforming Pipeline

```
SAI3_B RX DMA → audio_buffer_3b_rx (interleaved L,R,L,R...)
                       ↓ Process_ADC_Half()
                 delay_line_L[1024] / delay_line_R[1024]
                       ↓ Fill_Audio_Beamform(buf, samples, ch_offset)
    Per channel: read delay_line[write_pos - delay] → * gain >> 8 → * master >> 8 → mute
                       ↓
    SAI1_A(ch0-7) / SAI1_B(ch8-15) / SAI2_A(ch16-23) / SAI2_B(ch24-31)
```

- Delay line: 1024 samples circular buffer (power-of-2 fast modulo)
- MAX_DELAY: 64 samples (~1.33ms @ 48kHz = ~45.7cm at 343 m/s)
- Dual beam ready: delay_line_R populated, not yet used in output
