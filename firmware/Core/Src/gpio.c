/**
 * @file gpio.c
 * @brief GPIO configuration for Domosonica DS
 */

#include "main.h"

/**
 * @brief Initialize all GPIO pins
 */
void GPIO_Init_All(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable all GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    // ========================================================================
    // Status LED (PB5) - Output Push-Pull
    // ========================================================================
    GPIO_InitStruct.Pin = LED_STATUS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_STATUS_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LED_STATUS_PORT, LED_STATUS_PIN, GPIO_PIN_RESET);

    // ========================================================================
    // DAC Control Pins
    // ========================================================================

    // DAC Reset pins (PA6, PA7) - Output Push-Pull, active HIGH
    GPIO_InitStruct.Pin = DAC0_RST_PIN | DAC1_RST_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    // Start with DACs in reset
    HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DAC1_RST_PORT, DAC1_RST_PIN, GPIO_PIN_RESET);

    // DAC0 Mute Input (PD1) - Output Push-Pull, active LOW
    GPIO_InitStruct.Pin = DAC0_AMUTEI_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(DAC0_AMUTEI_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC0_AMUTEI_PORT, DAC0_AMUTEI_PIN, GPIO_PIN_SET); // Unmuted

    // DAC1 Mute Input (PB1) - Output Push-Pull, active LOW
    GPIO_InitStruct.Pin = DAC1_AMUTEI_PIN;
    HAL_GPIO_Init(DAC1_AMUTEI_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC1_AMUTEI_PORT, DAC1_AMUTEI_PIN, GPIO_PIN_SET); // Unmuted

    // DAC0 Mute Output (PD3) - Input
    GPIO_InitStruct.Pin = DAC0_AMUTEO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DAC0_AMUTEO_PORT, &GPIO_InitStruct);

    // DAC1 Mute Output (PC5) - Input
    GPIO_InitStruct.Pin = DAC1_AMUTEO_PIN;
    HAL_GPIO_Init(DAC1_AMUTEO_PORT, &GPIO_InitStruct);

    // ========================================================================
    // ADC Control Pins
    // ========================================================================

    // ADC Power Down (PA1) - Output, HIGH = normal operation
    GPIO_InitStruct.Pin = ADC_PDWN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ADC_PDWN_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ADC_PDWN_PORT, ADC_PDWN_PIN, GPIO_PIN_SET); // Normal operation

    // ADC OSR Select (PB0) - Output, HIGH = 128x oversampling
    GPIO_InitStruct.Pin = ADC_OSR_PIN;
    HAL_GPIO_Init(ADC_OSR_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ADC_OSR_PORT, ADC_OSR_PIN, GPIO_PIN_SET); // 128x oversampling

    // ADC HPF Bypass (PC3) - Output, LOW = HPF active
    GPIO_InitStruct.Pin = ADC_BYPASS_PIN;
    HAL_GPIO_Init(ADC_BYPASS_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ADC_BYPASS_PORT, ADC_BYPASS_PIN, GPIO_PIN_RESET); // HPF active
}

/**
 * @brief Initialize SAI1 GPIO (Audio Output to DACs)
 *
 * SAI1_A: PE2 (MCLK), PE4 (FS), PE5 (SCK), PE6 (SD)
 * SAI1_B: PE7 (SD), PE8 (SCK), PE9 (FS)
 */
void GPIO_SAI1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();

    // SAI1_MCLK_A (PE2) - shared master clock
    GPIO_InitStruct.Pin = SAI1_MCLK_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_SAI1;
    HAL_GPIO_Init(SAI1_MCLK_PORT, &GPIO_InitStruct);

    // SAI1_A: FS (PE4), SCK (PE5), SD (PE6)
    GPIO_InitStruct.Pin = SAI1_FS_A_PIN | SAI1_SCK_A_PIN | SAI1_SD_A_PIN;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // SAI1_B: SD (PE7), SCK (PE8), FS (PE9)
    GPIO_InitStruct.Pin = SAI1_SD_B_PIN | SAI1_SCK_B_PIN | SAI1_FS_B_PIN;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

/**
 * @brief Initialize SAI4 GPIO (Audio Input from ADC)
 *
 * SAI4_B: PC0 (FS), PC1 (SCK), PB11 (SD)
 */
void GPIO_SAI4_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // SAI4_FS_B (PC0), SAI4_SCK_B (PC1) - clock outputs
    GPIO_InitStruct.Pin = SAI4_FS_B_PIN | SAI4_SCK_B_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_SAI4;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // SAI4_SD_B (PB11) - data input
    GPIO_InitStruct.Pin = SAI4_SD_B_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF10_SAI4;
    HAL_GPIO_Init(SAI4_SD_B_PORT, &GPIO_InitStruct);
}

/**
 * @brief Initialize I2C1 GPIO
 *
 * PB8 (SCL), PB9 (SDA) - Open Drain with external pullups
 */
void GPIO_I2C1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = I2C1_SCL_PIN | I2C1_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;  // External 5.1k pullups
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/**
 * @brief Initialize UART7 GPIO
 *
 * PA8 (RX), PA15 (TX)
 */
void GPIO_UART7_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    // UART7_TX (PA15)
    GPIO_InitStruct.Pin = UART7_TX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_UART7;
    HAL_GPIO_Init(UART7_TX_PORT, &GPIO_InitStruct);

    // UART7_RX (PA8)
    GPIO_InitStruct.Pin = UART7_RX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Alternate = GPIO_AF11_UART7;
    HAL_GPIO_Init(UART7_RX_PORT, &GPIO_InitStruct);
}

/**
 * @brief Release DACs from reset
 */
void DAC_Release_Reset(void)
{
    HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC1_RST_PORT, DAC1_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(10); // Wait for DACs to initialize
}

/**
 * @brief Put DACs in reset
 */
void DAC_Assert_Reset(void)
{
    HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DAC1_RST_PORT, DAC1_RST_PIN, GPIO_PIN_RESET);
}

/**
 * @brief Mute/unmute DACs
 * @param mute 1 = mute, 0 = unmute
 */
void DAC_Set_Mute(uint8_t mute)
{
    GPIO_PinState state = mute ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(DAC0_AMUTEI_PORT, DAC0_AMUTEI_PIN, state);
    HAL_GPIO_WritePin(DAC1_AMUTEI_PORT, DAC1_AMUTEI_PIN, state);
}
