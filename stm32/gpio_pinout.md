# Pin Alternate Functions - STM32H743VIT6

## Configuración de GPIO para Audio

### SAI1 - Audio Output (DACs)

| Pin  | Function      | GPIO_AFx   | Periférico | Dirección | Notas |
|------|---------------|------------|------------|-----------|-------|
| PE2  | SAI1_MCLK_A   | GPIO_AF6   | SAI1       | Output    | Master clock compartido por DAC0, DAC1, ADC. R33 serie |
| PE4  | SAI1_FS_A     | GPIO_AF6   | SAI1       | Output    | Frame sync (LRCK) para DAC0 |
| PE5  | SAI1_SCK_A    | GPIO_AF6   | SAI1       | Output    | Bit clock (BCK) para DAC0. R33 serie |
| PE6  | SAI1_SD_A     | GPIO_AF6   | SAI1       | Output    | Serial data hacia DAC0 (DIN1) |
| PE7  | SAI1_SD_B     | GPIO_AF6   | SAI1       | Output    | Serial data hacia DAC1 (DIN1) |
| PE8  | SAI1_SCK_B    | GPIO_AF6   | SAI1       | Output    | Bit clock para DAC1. R33 serie |
| PE9  | SAI1_FS_B     | GPIO_AF6   | SAI1       | Output    | Frame sync para DAC1 |

### SAI4 - Audio Input (ADC)

| Pin  | Function      | GPIO_AFx   | Periférico | Dirección | Notas |
|------|---------------|------------|------------|-----------|-------|
| PC0  | SAI4_FS_B     | GPIO_AF10  | SAI4       | Output/Input | Frame sync - Master: Output, Slave: Input |
| PC1  | SAI4_SCK_B    | GPIO_AF10  | SAI4       | Output/Input | Bit clock - Master: Output, Slave: Input |
| PB11 | SAI4_SD_B     | GPIO_AF10  | SAI4       | Input     | Serial data desde ADC (DOUT) |

### I2C1 - Control Interface

| Pin  | Function      | GPIO_AFx   | Periférico | Dirección | Notas |
|------|---------------|------------|------------|-----------|-------|
| PB8  | I2C1_SCL      | GPIO_AF4   | I2C1       | OD        | Open-drain con pullup 5.1kΩ a 3V3 |
| PB9  | I2C1_SDA      | GPIO_AF4   | I2C1       | OD        | Open-drain con pullup 5.1kΩ a 3V3 |

### UART7 - ESP32 Communication

| Pin  | Function      | GPIO_AFx   | Periférico | Dirección | Notas |
|------|---------------|------------|------------|-----------|-------|
| PA8  | UART7_RX      | GPIO_AF11  | UART7      | Input     | Con R220 serie. Nivel 3.3V |
| PA15 | UART7_TX      | GPIO_AF11  | UART7      | Output    | Con R220 serie. Nivel 3.3V |

### SDMMC - MicroSD Card

| Pin  | Function      | GPIO_AFx   | Periférico | Dirección | Notas |
|------|---------------|------------|------------|-----------|-------|
| PC8  | SDMMC_D0      | GPIO_AF12  | SDMMC      | I/O       | Data line 0 |
| PC9  | SDMMC_D1      | GPIO_AF12  | SDMMC      | I/O       | Data line 1 |
| PC10 | SDMMC_D2      | GPIO_AF12  | SDMMC      | I/O       | Data line 2 |
| PC11 | SDMMC_D3      | GPIO_AF12  | SDMMC      | I/O       | Data line 3 |
| PC12 | SDMMC_CK      | GPIO_AF12  | SDMMC      | Output    | Clock |
| PD2  | SDMMC_CMD     | GPIO_AF12  | SDMMC      | I/O       | Command |

### LCD Interface (GPIO bitbanged)

| Pin  | Function      | GPIO_AFx   | Periférico | Dirección | Notas |
|------|---------------|------------|------------|-----------|-------|
| PE10 | LCD_LED       | GPIO       | -          | Output    | Backlight control |
| PE11 | LCD_CS        | GPIO       | -          | Output    | Chip select |
| PE12 | LCD_SCL       | GPIO       | -          | Output    | Serial clock |
| PE13 | LCD_WR_RS     | GPIO       | -          | Output    | Write/Register select |
| PE14 | LCD_SDA       | GPIO       | -          | Output    | Serial data |

### Control GPIO

#### DAC Control
| Pin  | Function      | GPIO_AFx   | Periférico | Dirección | Notas |
|------|---------------|------------|------------|-----------|-------|
| PA6  | DAC0_RST      | GPIO       | -          | Output    | Reset activo alto del PCM1690 #0 |
| PA7  | DAC1_RST      | GPIO       | -          | Output    | Reset activo alto del PCM1690 #1 |
| PD1  | DAC0_AMUTEI   | GPIO       | -          | Output    | Mute input activo bajo para DAC0 |
| PD3  | DAC0_AMUTEO   | GPIO       | -          | Input     | Mute status output de DAC0 |
| PB1  | DAC1_AMUTEI   | GPIO       | -          | Output    | Mute input activo bajo para DAC1 |
| PC5  | DAC1_AMUTEO   | GPIO       | -          | Input     | Mute status output de DAC1 |

#### ADC Control
| Pin  | Function      | GPIO_AFx   | Periférico | Dirección | Notas |
|------|---------------|------------|------------|-----------|-------|
| PA1  | ADC_PDWN      | GPIO       | -          | Output    | Power down del PCM1802 |
| PB0  | ADC_OSR       | GPIO       | -          | Output    | Oversample ratio select |
| PC3  | ADC_BYPASS    | GPIO       | -          | Output    | HPF bypass control |

#### Status/Debug
| Pin  | Function      | GPIO_AFx   | Periférico | Dirección | Notas |
|------|---------------|------------|------------|-----------|-------|
| PB5  | LED_STATUS    | GPIO       | -          | Output    | LED de estado del sistema |

### Optional Header (J1)
| Pin  | Function      | GPIO_AFx   | Periférico | Dirección | Notas |
|------|---------------|------------|------------|-----------|-------|
| PA9  | J1_Pin1       | GPIO       | -          | I/O       | Disponible para expansión |
| PD15 | J1_Pin2       | GPIO       | -          | I/O       | Disponible para expansión |
| PC7  | J1_Pin3       | GPIO       | -          | I/O       | Disponible para expansión |
| PD9  | J1_Pin4       | GPIO       | -          | I/O       | Disponible para expansión |
| PB15 | J1_Pin5       | GPIO       | -          | I/O       | Disponible para expansión |
| PB13 | J1_Pin6       | GPIO       | -          | I/O       | Disponible para expansión |

---

## HAL Configuration Code Examples

### SAI1_A GPIO Init (Master TX para DAC0)
```c
GPIO_InitTypeDef GPIO_InitStruct = {0};

// Enable clocks
__HAL_RCC_GPIOE_CLK_ENABLE();

// Configure SAI1_MCLK_A (PE2)
GPIO_InitStruct.Pin = GPIO_PIN_2;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
GPIO_InitStruct.Alternate = GPIO_AF6_SAI1;
HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

// Configure SAI1_FS_A (PE4), SAI1_SCK_A (PE5), SAI1_SD_A (PE6)
GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
GPIO_InitStruct.Alternate = GPIO_AF6_SAI1;
HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
```

### SAI1_B GPIO Init (Master TX para DAC1, sincronizado)
```c
// Configure SAI1_SD_B (PE7), SAI1_SCK_B (PE8), SAI1_FS_B (PE9)
GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
GPIO_InitStruct.Alternate = GPIO_AF6_SAI1;
HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
```

### SAI4_B GPIO Init (Master RX para ADC)
```c
// Enable clocks
__HAL_RCC_GPIOB_CLK_ENABLE();
__HAL_RCC_GPIOC_CLK_ENABLE();

// Configure SAI4_FS_B (PC0), SAI4_SCK_B (PC1)
GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;  // Output si Master
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
GPIO_InitStruct.Alternate = GPIO_AF10_SAI4;
HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

// Configure SAI4_SD_B (PB11)
GPIO_InitStruct.Pin = GPIO_PIN_11;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;  // Input data
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
GPIO_InitStruct.Alternate = GPIO_AF10_SAI4;
HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
```

### I2C1 GPIO Init
```c
// Enable clocks
__HAL_RCC_GPIOB_CLK_ENABLE();

// Configure I2C1_SCL (PB8), I2C1_SDA (PB9)
GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;  // Open-drain
GPIO_InitStruct.Pull = GPIO_NOPULL;      // External pullup 5.1kΩ
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
```

### Control GPIO Init (DACs y ADC)
```c
// Enable clocks
__HAL_RCC_GPIOA_CLK_ENABLE();
__HAL_RCC_GPIOB_CLK_ENABLE();
__HAL_RCC_GPIOC_CLK_ENABLE();
__HAL_RCC_GPIOD_CLK_ENABLE();

// DAC Reset pins (PA6, PA7) - Output Push-Pull
GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

// DAC Mute outputs (PD1, PB1) - Output Push-Pull
GPIO_InitStruct.Pin = GPIO_PIN_1;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

// DAC Mute inputs (PD3, PC5) - Input
GPIO_InitStruct.Pin = GPIO_PIN_3;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_NOPULL;
HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

GPIO_InitStruct.Pin = GPIO_PIN_5;
HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

// ADC Control pins (PA1, PB0, PC3) - Output Push-Pull
GPIO_InitStruct.Pin = GPIO_PIN_1;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

GPIO_InitStruct.Pin = GPIO_PIN_0;
HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

GPIO_InitStruct.Pin = GPIO_PIN_3;
HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

// Set initial states
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);  // DAC0_RST low
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);  // DAC1_RST low
HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_SET);    // DAC0_AMUTEI high (unmuted)
HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);    // DAC1_AMUTEI high (unmuted)
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);    // ADC_PDWN high (normal operation)
HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);    // ADC_OSR high (128x oversampling)
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);  // ADC_BYPASS low (HPF active)
```

---

## Pines Libres para Expansión

| Puerto | Pines Libres | Total |
|--------|--------------|-------|
| PA     | PA0, PA2-5, PA10-12 | 8 |
| PB     | PB2-4, PB6-7, PB10, PB12, PB14 | 8 |
| PC     | PC2, PC4, PC6, PC13 | 4 |
| PD     | PD0, PD4-8, PD10-14 | 10 |
| PE     | PE0-1, PE3, PE15 | 4 |

**Total:** 34 pines libres disponibles para futuras expansiones
