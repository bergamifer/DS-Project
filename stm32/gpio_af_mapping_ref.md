# GPIO Alternate Function Mapping - STM32H743

**Source:** RM0433 Reference Manual + STM32H743 Datasheet
**Note:** Valores basados en conocimiento del STM32H7. Verificar en Datasheet tabla de alternate functions.

---

## Table of Contents
- [Alternate Function Summary](#alternate-function-summary)
- [GPIO Configuration Registers](#gpio-configuration-registers)
- [AF Mapping por Puerto](#af-mapping-por-puerto)
- [Configuration Code](#configuration-code)

---

## Alternate Function Summary

### Project Pin Usage

| Pin | Default | AFx | Peripheral | Function | Notes |
|-----|---------|-----|------------|----------|-------|
| **SAI1 (Audio DACs)** | | | | | |
| PE2 | GPIO | AF6 | SAI1 | MCLK_A | Master clock output |
| PE4 | GPIO | AF6 | SAI1 | FS_A | Frame sync (LRCK) |
| PE5 | GPIO | AF6 | SAI1 | SCK_A | Bit clock (BCK) |
| PE6 | GPIO | AF6 | SAI1 | SD_A | Serial data |
| PE7 | GPIO | AF6 | SAI1 | SD_B | Serial data |
| PE8 | GPIO | AF6 | SAI1 | SCK_B | Bit clock |
| PE9 | GPIO | AF6 | SAI1 | FS_B | Frame sync |
| **SAI4 (Audio ADC)** | | | | | |
| PC0 | GPIO | AF10 | SAI4 | FS_B | Frame sync |
| PC1 | GPIO | AF10 | SAI4 | SCK_B | Bit clock |
| PB11 | GPIO | AF10 | SAI4 | SD_B | Serial data |
| **I2C1 (Control)** | | | | | |
| PB8 | GPIO | AF4 | I2C1 | SCL | Open-drain, 5.1kΩ pullup |
| PB9 | GPIO | AF4 | I2C1 | SDA | Open-drain, 5.1kΩ pullup |
| **UART7 (ESP32)** | | | | | |
| PA8 | GPIO | AF11 | UART7 | RX | With R220 series |
| PA15 | GPIO | AF11 | UART7 | TX | With R220 series |
| **SDMMC (MicroSD)** | | | | | |
| PC8 | GPIO | AF12 | SDMMC1 | D0 | Data line 0 |
| PC9 | GPIO | AF12 | SDMMC1 | D1 | Data line 1 |
| PC10 | GPIO | AF12 | SDMMC1 | D2 | Data line 2 |
| PC11 | GPIO | AF12 | SDMMC1 | D3 | Data line 3 |
| PC12 | GPIO | AF12 | SDMMC1 | CK | Clock |
| PD2 | GPIO | AF12 | SDMMC1 | CMD | Command |

---

## GPIO Configuration Registers

### Register Summary per Port

| Register | Offset | Description | Size |
|----------|--------|-------------|------|
| GPIOx_MODER | 0x00 | Mode (input/output/AF/analog) | 2 bits/pin |
| GPIOx_OTYPER | 0x04 | Output type (PP/OD) | 1 bit/pin |
| GPIOx_OSPEEDR | 0x08 | Output speed | 2 bits/pin |
| GPIOx_PUPDR | 0x0C | Pull-up/pull-down | 2 bits/pin |
| GPIOx_IDR | 0x10 | Input data (read-only) | 1 bit/pin |
| GPIOx_ODR | 0x14 | Output data | 1 bit/pin |
| GPIOx_BSRR | 0x18 | Bit set/reset (write-only) | 32 bits |
| GPIOx_LCKR | 0x1C | Lock register | - |
| GPIOx_AFRL | 0x20 | AF selection pins 0-7 | 4 bits/pin |
| GPIOx_AFRH | 0x24 | AF selection pins 8-15 | 4 bits/pin |

### GPIOx_MODER - Mode Register

| Value | Mode | Description |
|-------|------|-------------|
| 00 | Input | Default after reset |
| 01 | General purpose output | Push-pull or open-drain |
| **10** | **Alternate function** | **For peripherals** |
| 11 | Analog | ADC/DAC |

### GPIOx_OTYPER - Output Type Register

| Value | Type | Use |
|-------|------|-----|
| 0 | Push-pull | **SAI, UART, SDMMC** |
| 1 | Open-drain | **I2C** |

### GPIOx_OSPEEDR - Output Speed Register

| Value | Speed | Use |
|-------|-------|-----|
| 00 | Low | GPIO control |
| 01 | Medium | - |
| 10 | High | I2C |
| **11** | **Very High** | **SAI, UART, SDMMC** |

### GPIOx_PUPDR - Pull-up/Pull-down Register

| Value | Configuration | Use |
|-------|---------------|-----|
| **00** | **No pull** | **Most peripherals** (external pullup on I2C) |
| 01 | Pull-up | Optional |
| 10 | Pull-down | Optional |
| 11 | Reserved | - |

### GPIOx_AFRL / GPIOx_AFRH - Alternate Function Registers

4 bits per pin to select AF0-AF15:
| AFx | Typical Use |
|-----|-------------|
| AF0 | RTC, MCO, TRACE |
| AF1 | TIM1, TIM2 |
| AF2 | TIM3-5 |
| AF3 | TIM8, LPTIM |
| **AF4** | **I2C1-4, USART1** |
| AF5 | SPI1-6 |
| **AF6** | **SAI1, SAI2, SPI** |
| AF7 | USART1-3, UART7 |
| AF8 | UART4-8, LPUART |
| AF9 | FDCAN, QUADSPI |
| **AF10** | **SAI4, QUADSPI, OctoSPI** |
| **AF11** | **UART7**, ETH |
| **AF12** | **SDMMC, FMC** |
| AF13 | DCMI |
| AF14 | LTDC |
| AF15 | EVENTOUT |

---

## AF Mapping por Puerto

### Port A (PA0-PA15)

| Pin | AF4 | AF6 | AF7 | AF8 | AF11 | AF12 |
|-----|-----|-----|-----|-----|------|------|
| PA8 | I2C3_SCL | SAI1_SCK_B | USART1_CK | UART7_RX | UART7_RX ⭐ | - |
| PA15 | UART7_TX | SAI2_FS_B | - | - | UART7_TX ⭐ | - |

### Port B (PB0-PB15)

| Pin | AF4 | AF6 | AF10 | Other |
|-----|-----|-----|------|-------|
| PB8 | I2C1_SCL ⭐ | UART4_RX | - | SDMMC1_D4 |
| PB9 | I2C1_SDA ⭐ | - | - | SDMMC1_D5 |
| PB11 | I2C2_SDA | - | SAI4_SD_B ⭐ | USART3_RX |

### Port C (PC0-PC15)

| Pin | AF8 | AF10 | AF12 | Other |
|-----|-----|------|------|-------|
| PC0 | - | SAI4_FS_B ⭐ | - | - |
| PC1 | - | SAI4_SCK_B ⭐ | - | - |
| PC8 | UART5_RTS | - | SDMMC1_D0 ⭐ | - |
| PC9 | I2C3_SDA | - | SDMMC1_D1 ⭐ | - |
| PC10 | - | - | SDMMC1_D2 ⭐ | USART3_TX |
| PC11 | - | - | SDMMC1_D3 ⭐ | USART3_RX |
| PC12 | - | - | SDMMC1_CK ⭐ | UART5_TX |

### Port D (PD0-PD15)

| Pin | AF12 | Other |
|-----|------|-------|
| PD2 | SDMMC1_CMD ⭐ | UART5_RX |

### Port E (PE0-PE15)

| Pin | AF6 | Other |
|-----|-----|-------|
| PE2 | SAI1_MCLK_A ⭐ | QUADSPI_BK1_IO2 |
| PE4 | SAI1_FS_A ⭐ | - |
| PE5 | SAI1_SCK_A ⭐ | - |
| PE6 | SAI1_SD_A ⭐ | - |
| PE7 | SAI1_SD_B ⭐ | UART7_RX |
| PE8 | SAI1_SCK_B ⭐ | UART7_TX |
| PE9 | SAI1_FS_B ⭐ | - |

---

## Configuration Code

### SAI1 GPIO Init

```c
void SAI1_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable GPIOE clock
    __HAL_RCC_GPIOE_CLK_ENABLE();

    // Configure PE2 (SAI1_MCLK_A)
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_SAI1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // Configure PE4-6 (SAI1_A: FS, SCK, SD)
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_SAI1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // Configure PE7-9 (SAI1_B: SD, SCK, FS)
    GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_SAI1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}
```

### SAI4 GPIO Init

```c
void SAI4_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable clocks
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // Configure PC0-1 (SAI4_FS_B, SAI4_SCK_B)
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_SAI4;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // Configure PB11 (SAI4_SD_B)
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_SAI4;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
```

### I2C1 GPIO Init

```c
void I2C1_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable GPIOB clock
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Configure PB8-9 (I2C1_SCL, I2C1_SDA)
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;  // Open-drain!
    GPIO_InitStruct.Pull = GPIO_NOPULL;      // External pullup
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
```

### Manual Register Configuration (Low-Level)

```c
void SAI1_GPIO_Init_LowLevel(void)
{
    // Enable GPIOE clock
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;

    // Configure PE2, PE4-9 as AF mode (10)
    GPIOE->MODER &= ~(GPIO_MODER_MODE2_Msk |
                      GPIO_MODER_MODE4_Msk | GPIO_MODER_MODE5_Msk |
                      GPIO_MODER_MODE6_Msk | GPIO_MODER_MODE7_Msk |
                      GPIO_MODER_MODE8_Msk | GPIO_MODER_MODE9_Msk);
    GPIOE->MODER |= (GPIO_MODE_AF_PP << GPIO_MODER_MODE2_Pos) |
                     (GPIO_MODE_AF_PP << GPIO_MODER_MODE4_Pos) |
                     (GPIO_MODE_AF_PP << GPIO_MODER_MODE5_Pos) |
                     (GPIO_MODE_AF_PP << GPIO_MODER_MODE6_Pos) |
                     (GPIO_MODE_AF_PP << GPIO_MODER_MODE7_Pos) |
                     (GPIO_MODE_AF_PP << GPIO_MODER_MODE8_Pos) |
                     (GPIO_MODE_AF_PP << GPIO_MODER_MODE9_Pos);

    // Set output type to push-pull (already 0 by default)
    GPIOE->OTYPER &= ~(GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5 |
                        GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);

    // Set speed to very high (11)
    GPIOE->OSPEEDR |= (GPIO_SPEED_FREQ_VERY_HIGH << GPIO_OSPEEDR_OSPEED2_Pos) |
                       (GPIO_SPEED_FREQ_VERY_HIGH << GPIO_OSPEEDR_OSPEED4_Pos) |
                       (GPIO_SPEED_FREQ_VERY_HIGH << GPIO_OSPEEDR_OSPEED5_Pos) |
                       (GPIO_SPEED_FREQ_VERY_HIGH << GPIO_OSPEEDR_OSPEED6_Pos) |
                       (GPIO_SPEED_FREQ_VERY_HIGH << GPIO_OSPEEDR_OSPEED7_Pos) |
                       (GPIO_SPEED_FREQ_VERY_HIGH << GPIO_OSPEEDR_OSPEED8_Pos) |
                       (GPIO_SPEED_FREQ_VERY_HIGH << GPIO_OSPEEDR_OSPEED9_Pos);

    // No pull-up/pull-down
    GPIOE->PUPDR &= ~(GPIO_PUPDR_PUPD2_Msk | GPIO_PUPDR_PUPD4_Msk |
                       GPIO_PUPDR_PUPD5_Msk | GPIO_PUPDR_PUPD6_Msk |
                       GPIO_PUPDR_PUPD7_Msk | GPIO_PUPDR_PUPD8_Msk |
                       GPIO_PUPDR_PUPD9_Msk);

    // Set alternate function AF6 (SAI1)
    // AFRL for pins 0-7 (PE2, PE4-7)
    GPIOE->AFR[0] &= ~(GPIO_AFRL_AFSEL2_Msk | GPIO_AFRL_AFSEL4_Msk |
                        GPIO_AFRL_AFSEL5_Msk | GPIO_AFRL_AFSEL6_Msk |
                        GPIO_AFRL_AFSEL7_Msk);
    GPIOE->AFR[0] |= (6 << GPIO_AFRL_AFSEL2_Pos) |   // PE2 = AF6
                      (6 << GPIO_AFRL_AFSEL4_Pos) |   // PE4 = AF6
                      (6 << GPIO_AFRL_AFSEL5_Pos) |   // PE5 = AF6
                      (6 << GPIO_AFRL_AFSEL6_Pos) |   // PE6 = AF6
                      (6 << GPIO_AFRL_AFSEL7_Pos);    // PE7 = AF6

    // AFRH for pins 8-15 (PE8-9)
    GPIOE->AFR[1] &= ~(GPIO_AFRH_AFSEL8_Msk | GPIO_AFRH_AFSEL9_Msk);
    GPIOE->AFR[1] |= (6 << GPIO_AFRH_AFSEL8_Pos) |   // PE8 = AF6
                      (6 << GPIO_AFRH_AFSEL9_Pos);    // PE9 = AF6
}
```

---

## Quick Reference: AF Numbers

| Peripheral | Alternate Function Number |
|------------|---------------------------|
| SAI1 | **AF6** |
| SAI4 | **AF10** |
| I2C1 | **AF4** |
| UART7 | **AF11** |
| SDMMC1 | **AF12** |

---

## GPIO Speed vs Frequency

| GPIO Speed | Max Toggle Freq | Use Case |
|------------|-----------------|----------|
| Low | ~2 MHz | Control GPIO |
| Medium | ~25 MHz | General purpose |
| High | ~50 MHz | I2C, slower SPI |
| **Very High** | **~100 MHz** | **SAI, SDMMC, fast SPI** |

**Note:** For SAI @ 12.288 MHz MCLK, Very High speed is required

---

## Verification Checklist

- [ ] All SAI pins configured as AF6 (SAI1) or AF10 (SAI4)
- [ ] I2C pins configured as open-drain with external pullup
- [ ] All high-speed signals (SAI, SDMMC) set to Very High speed
- [ ] No internal pull-up/pull-down on SAI pins (external resistors if needed)
- [ ] UART pins with series resistor (R220) for protection
- [ ] GPIO clocks enabled before configuration

---

**Note:** Valores basados en conocimiento del STM32H743. Para verificar AF mapping exacto de cada pin, consultar STM32H743 Datasheet Table 7 "Alternate function mapping".
