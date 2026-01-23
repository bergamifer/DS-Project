# System Initialization Sequence - Domosonica DS

## Overview
Este documento describe la secuencia completa de inicialización del firmware, desde power-on hasta el comienzo del audio streaming.

---

## Phase 1: System Startup (HAL Layer)

### 1.1 Clock Configuration
```c
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // Enable HSE (25 MHz external crystal from WeAct board)
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

    // PLL1: System clock (480 MHz for CPU)
    RCC_OscInitStruct.PLL.PLLM = 5;    // 25 MHz / 5 = 5 MHz
    RCC_OscInitStruct.PLL.PLLN = 192;  // 5 MHz × 192 = 960 MHz VCO
    RCC_OscInitStruct.PLL.PLLP = 2;    // 960 MHz / 2 = 480 MHz
    RCC_OscInitStruct.PLL.PLLQ = 4;    // 960 MHz / 4 = 240 MHz
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;  // 4-8 MHz
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;  // Wide range
    RCC_OscInitStruct.PLL.PLLFRACN = 0;

    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    // System clock = PLL1
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                   RCC_CLOCKTYPE_SYSCLK |
                                   RCC_CLOCKTYPE_PCLK1 |
                                   RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;     // 240 MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;    // 120 MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;    // 120 MHz

    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
}
```

### 1.2 PLL3 Configuration (Audio Clock)
```c
void Audio_PLL3_Config(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    // Configure PLL3 for 12.288 MHz audio clock
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI1 |
                                                RCC_PERIPHCLK_SAI4;

    PeriphClkInitStruct.PLL3.PLL3M = 5;     // 25 MHz / 5 = 5 MHz ref
    PeriphClkInitStruct.PLL3.PLL3N = 68;    // 5 MHz × 68 = 340 MHz VCO
    PeriphClkInitStruct.PLL3.PLL3P = 28;    // 340 MHz / 28 = 12.142 MHz
    PeriphClkInitStruct.PLL3.PLL3Q = 2;
    PeriphClkInitStruct.PLL3.PLL3R = 2;
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;

    // Fine-tune with fractional mode for exactly 12.288 MHz
    PeriphClkInitStruct.PLL3.PLL3FRACN = 4096;  // Ajustar según necesidad

    // Select PLL3_P as SAI clock source
    PeriphClkInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL3;
    PeriphClkInitStruct.Sai4AClockSelection = RCC_SAI4ACLKSOURCE_PLL3;

    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
}
```

---

## Phase 2: Peripheral Initialization

### 2.1 GPIO Configuration
```c
void GPIO_Init_All(void)
{
    // Enable all GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    // 1. SAI GPIO (see pin_alternate_functions.md)
    SAI1_GPIO_Init();
    SAI4_GPIO_Init();

    // 2. I2C GPIO
    I2C1_GPIO_Init();

    // 3. Control GPIO (DACs, ADC)
    Control_GPIO_Init();

    // 4. Status LED
    LED_GPIO_Init();
}
```

### 2.2 I2C1 Initialization
```c
void I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x20404768;  // 100 kHz @ 120 MHz APB1
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    HAL_I2C_Init(&hi2c1);
}
```

### 2.3 DMA/BDMA Configuration
```c
void DMA_BDMA_Init(void)
{
    // Enable DMA clocks
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_BDMA_CLK_ENABLE();  // SAI4 usa BDMA (dominio D3)

    // DMA1 Stream 0: SAI1_A TX (DMAMUX1 request 87)
    hdma_sai1_a.Instance = DMA1_Stream0;
    hdma_sai1_a.Init.Request = DMA_REQUEST_SAI1_A;
    hdma_sai1_a.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_sai1_a.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_sai1_a.Init.MemInc = DMA_MINC_ENABLE;
    hdma_sai1_a.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_sai1_a.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_sai1_a.Init.Mode = DMA_CIRCULAR;
    hdma_sai1_a.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_sai1_a.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma_sai1_a.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL;
    HAL_DMA_Init(&hdma_sai1_a);
    __HAL_LINKDMA(&hsai1_a, hdmatx, hdma_sai1_a);

    // DMA1 Stream 5: SAI1_B TX (DMAMUX1 request 88)
    hdma_sai1_b.Instance = DMA1_Stream5;
    hdma_sai1_b.Init.Request = DMA_REQUEST_SAI1_B;
    // ... similar config ...

    // ⚠️ BDMA Channel 7: SAI4_B RX (DMAMUX2 request 16)
    // SAI4 está en dominio D3, DEBE usar BDMA (no DMA1/DMA2)
    // Ver dma_mapping_ref.md para detalles
    hdma_sai4_b.Instance = BDMA_Channel7;
    hdma_sai4_b.Init.Request = BDMA_REQUEST_SAI4_B;
    hdma_sai4_b.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_sai4_b.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_sai4_b.Init.MemInc = DMA_MINC_ENABLE;
    hdma_sai4_b.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_sai4_b.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_sai4_b.Init.Mode = DMA_CIRCULAR;
    hdma_sai4_b.Init.Priority = DMA_PRIORITY_MEDIUM;
    // BDMA no tiene FIFO - omitir configuración FIFO
    HAL_DMA_Init(&hdma_sai4_b);
    __HAL_LINKDMA(&hsai4_b, hdmarx, hdma_sai4_b);

    // DMA/BDMA Interrupts
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_SetPriority(BDMA_Channel7_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(BDMA_Channel7_IRQn);
}
```

### 2.4 SAI Configuration

#### SAI1_A (DAC0 - Master Asynchronous)
```c
void SAI1_A_Init(void)
{
    hsai1_a.Instance = SAI1_Block_A;
    hsai1_a.Init.AudioMode = SAI_MODEMASTER_TX;
    hsai1_a.Init.Synchro = SAI_ASYNCHRONOUS;
    hsai1_a.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
    hsai1_a.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
    hsai1_a.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_HF;
    hsai1_a.Init.AudioFrequency = 48000;
    hsai1_a.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
    hsai1_a.Init.MonoStereoMode = SAI_STEREOMODE;
    hsai1_a.Init.CompandingMode = SAI_NOCOMPANDING;

    // Frame config (TDM 8 slots)
    hsai1_a.FrameInit.FrameLength = 256;        // 32 bits × 8 slots
    hsai1_a.FrameInit.ActiveFrameLength = 1;    // 1-bit pulse
    hsai1_a.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
    hsai1_a.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
    hsai1_a.FrameInit.FSOffset = SAI_FS_FIRSTBIT;

    // Slot config
    hsai1_a.SlotInit.FirstBitOffset = 0;
    hsai1_a.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
    hsai1_a.SlotInit.SlotNumber = 8;
    hsai1_a.SlotInit.SlotActive = 0x00FF;  // Slots 0-7

    HAL_SAI_Init(&hsai1_a);
}
```

#### SAI1_B (DAC1 - Master Synchronous)
```c
void SAI1_B_Init(void)
{
    hsai1_b.Instance = SAI1_Block_B;
    hsai1_b.Init.AudioMode = SAI_MODEMASTER_TX;
    hsai1_b.Init.Synchro = SAI_SYNCHRONOUS;  // ⭐ Synchronized with A
    hsai1_b.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
    hsai1_b.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_HF;
    hsai1_b.Init.MonoStereoMode = SAI_STEREOMODE;
    hsai1_b.Init.CompandingMode = SAI_NOCOMPANDING;

    // Frame config (igual que SAI1_A)
    hsai1_b.FrameInit.FrameLength = 256;
    hsai1_b.FrameInit.ActiveFrameLength = 1;
    hsai1_b.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
    hsai1_b.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
    hsai1_b.FrameInit.FSOffset = SAI_FS_FIRSTBIT;

    // Slot config
    hsai1_b.SlotInit.FirstBitOffset = 0;
    hsai1_b.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
    hsai1_b.SlotInit.SlotNumber = 8;
    hsai1_b.SlotInit.SlotActive = 0x00FF;

    HAL_SAI_Init(&hsai1_b);
}
```

#### SAI4_B (ADC - Master RX)
```c
void SAI4_B_Init(void)
{
    hsai4_b.Instance = SAI4_Block_B;
    hsai4_b.Init.AudioMode = SAI_MODEMASTER_RX;  // Master receiver
    hsai4_b.Init.Synchro = SAI_ASYNCHRONOUS;
    hsai4_b.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
    hsai4_b.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
    hsai4_b.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_HF;
    hsai4_b.Init.AudioFrequency = 48000;
    hsai4_b.Init.MonoStereoMode = SAI_STEREOMODE;
    hsai4_b.Init.CompandingMode = SAI_NOCOMPANDING;

    // Frame config (I2S 2 channels)
    hsai4_b.FrameInit.FrameLength = 64;         // 32 bits × 2 channels
    hsai4_b.FrameInit.ActiveFrameLength = 32;   // Half frame
    hsai4_b.FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
    hsai4_b.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
    hsai4_b.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;  // I2S standard

    // Slot config
    hsai4_b.SlotInit.FirstBitOffset = 0;
    hsai4_b.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
    hsai4_b.SlotInit.SlotNumber = 2;
    hsai4_b.SlotInit.SlotActive = 0x0003;  // Slots 0-1 (L+R)

    HAL_SAI_Init(&hsai4_b);
}
```

---

## Phase 3: Audio Chip Initialization

### 3.1 PCM1690 (DAC) Initialization
```c
void PCM1690_Init(uint8_t i2c_addr)
{
    uint8_t reg_data[2];

    // 1. Software reset
    reg_data[0] = 0x40;  // Register address
    reg_data[1] = 0x80;  // SRST=1 (reset)
    HAL_I2C_Master_Transmit(&hi2c1, i2c_addr << 1, reg_data, 2, 100);
    HAL_Delay(10);

    // 2. Normal operation, TDM mode
    reg_data[1] = 0xC0;  // MRST=1, SRST=1 (normal)
    HAL_I2C_Master_Transmit(&hi2c1, i2c_addr << 1, reg_data, 2, 100);

    // 3. Audio interface format: I2S, 24-bit
    reg_data[0] = 0x41;  // Register 65
    reg_data[1] = 0x80;  // PSMDA=1 (power-save disabled), I2S 24-bit
    HAL_I2C_Master_Transmit(&hi2c1, i2c_addr << 1, reg_data, 2, 100);

    // 4. Set all channels to max volume (0xFF = 0dB)
    for (uint8_t chan = 0; chan < 8; chan++) {
        reg_data[0] = 0x48 + chan;  // Registers 72-79
        reg_data[1] = 0xFF;         // 0 dB attenuation
        HAL_I2C_Master_Transmit(&hi2c1, i2c_addr << 1, reg_data, 2, 100);
    }

    // 5. Enable all DACs (OPEDA=0x00)
    reg_data[0] = 0x42;  // Register 66
    reg_data[1] = 0x00;  // All DACs enabled
    HAL_I2C_Master_Transmit(&hi2c1, i2c_addr << 1, reg_data, 2, 100);
}
```

### 3.2 PCM1802 (ADC) GPIO Configuration
```c
void PCM1802_Init(void)
{
    // PCM1802 configurado via GPIO (strapping pins):
    // MODE0=GND, MODE1=GND → Slave mode
    // FMT0=3V3, FMT1=GND → I2S 24-bit

    // 1. Release from power-down
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);  // PDWN = HIGH
    HAL_Delay(10);

    // 2. Set oversampling ratio
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);  // OSR = HIGH (128x)

    // 3. Enable HPF (no bypass)
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);  // BYPASS = LOW

    // Wait for ADC to stabilize
    HAL_Delay(100);
}
```

### 3.3 Reset Sequence para DACs
```c
void DAC_Reset_Sequence(void)
{
    // 1. Assert reset (low)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);  // DAC0_RST
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);  // DAC1_RST
    HAL_Delay(10);

    // 2. Release reset (high)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(10);

    // 3. Initialize via I2C
    PCM1690_Init(0x4C);  // DAC0
    PCM1690_Init(0x4E);  // DAC1
}
```

---

## Phase 4: Audio Streaming Start

### 4.1 Enable Sequence
```c
void Audio_Start_Streaming(void)
{
    // Buffers
    uint32_t tx_buffer_dac0[8 * BUFFER_SIZE];  // 8 channels
    uint32_t tx_buffer_dac1[8 * BUFFER_SIZE];
    uint32_t rx_buffer_adc[2 * BUFFER_SIZE];   // 2 channels

    // Clear buffers
    memset(tx_buffer_dac0, 0, sizeof(tx_buffer_dac0));
    memset(tx_buffer_dac1, 0, sizeof(tx_buffer_dac1));
    memset(rx_buffer_adc, 0, sizeof(rx_buffer_adc));

    // Start in order: RX first, then TX
    // 1. Start ADC reception (SAI4_B)
    HAL_SAI_Receive_DMA(&hsai4_b, (uint8_t*)rx_buffer_adc,
                        2 * BUFFER_SIZE);

    HAL_Delay(1);  // Small delay

    // 2. Start DAC0 transmission (SAI1_A)
    HAL_SAI_Transmit_DMA(&hsai1_a, (uint8_t*)tx_buffer_dac0,
                         8 * BUFFER_SIZE);

    // 3. Start DAC1 transmission (SAI1_B - synchronized)
    HAL_SAI_Transmit_DMA(&hsai1_b, (uint8_t*)tx_buffer_dac1,
                         8 * BUFFER_SIZE);
}
```

### 4.2 DMA Callbacks
```c
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai->Instance == SAI1_Block_A) {
        // DAC0: Half of buffer transmitted
        // Process second half while first half is being transmitted
        Process_Audio_DAC0();
    }
    else if (hsai->Instance == SAI1_Block_B) {
        // DAC1: Process audio
        Process_Audio_DAC1();
    }
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai->Instance == SAI4_Block_B) {
        // ADC: New data received
        Process_Audio_ADC();
    }
}
```

---

## Complete Initialization Order

```
1. HAL_Init()
2. SystemClock_Config()           // CPU @ 480 MHz
3. Audio_PLL3_Config()             // PLL3 @ 12.288 MHz
4. GPIO_Init_All()                 // All GPIOs
5. I2C1_Init()                     // I2C @ 100 kHz
6. DMA_BDMA_Init()                 // DMA1 y BDMA (SAI4 usa BDMA)
7. SAI1_A_Init()                   // Master async
8. SAI1_B_Init()                   // Master sync
9. SAI4_B_Init()                   // Master RX
10. DAC_Reset_Sequence()           // Reset DACs
11. PCM1802_Init()                 // Configure ADC
12. Audio_Start_Streaming()        // Start audio flow
13. Main_Loop()                    // Enter main processing loop
```

---

## Timing Constraints

- **Power-on to audio:** < 500 ms
- **DAC reset pulse:** 10 ms minimum
- **ADC stabilization:** 100 ms after PDWN release
- **I2C transaction:** < 10 ms cada comando
- **SAI enable order:** SAI4 → SAI1_A → SAI1_B (delays de 1ms entre cada uno)

---

## Error Handling

### SAI Errors
- **Underrun (TX):** Log error, continue streaming (fill with zeros)
- **Overrun (RX):** Log error, discard oldest data
- **Timeout:** Reset SAI periférico

### I2C Errors
- **NACK:** Retry up to 3 times, then log fatal error
- **Timeout:** Reset I2C bus

### DMA Errors
- **Transfer error:** Stop streaming, reinitialize DMA
- **FIFO error:** Adjust FIFO threshold

---

## Debug Output

Durante inicialización, usar UART o ITM para debug:
```c
printf("System clock: %lu MHz\n", HAL_RCC_GetSysClockFreq() / 1000000);
printf("SAI1 clock: %lu Hz\n", HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1));
printf("I2C1 status: %d\n", hi2c1.State);
printf("Audio streaming started\n");
```

---

## Checklist Pre-Start

- [ ] HSE crystal oscilando @ 25 MHz
- [ ] PLL3 locked @ 12.288 MHz
- [ ] SAI1_A/B clocks configurados
- [ ] SAI4_B clock configurado
- [ ] I2C funcionando (test con lectura de registro)
- [ ] DACs respondiendo en I2C (addr 0x4C, 0x4E)
- [ ] ADC pins correctamente configurados
- [ ] DMA streams mapeados correctamente
- [ ] Buffers de audio inicializados en memoria correcta
- [ ] Interrupts habilitados (DMA, SAI)
- [ ] GPIO control pins en estado correcto
