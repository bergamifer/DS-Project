/**
 * @file gpio.c
 * @brief GPIO configuration for Domosonica DS9 v2.0
 *
 * Hardware: STM32H743VIT6 LQFP100
 * Version: 2.0 - Updated for 32-channel beamforming system
 */

#include "main.h"
#include "pin_definitions.h"

/**
 * @brief Initialize all GPIO pins for DS9 v2.0
 *
 * Configures:
 * - 4 Status LEDs (PC4, PC5, PB0, PB1)
 * - 4 DAC reset pins (PE10, PE3, PC10, PB15)
 * - 4 DAC mute input/output pairs
 * - 8 ADC control pins (extended in v2.0)
 * - Power enable (PA10)
 * - EEPROM write protect (PB5)
 * - Test points (PE13, PE14, PA8)
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
    // Status LEDs (PC4, PC5, PB0, PB1) - Output Push-Pull, Active HIGH
    // ========================================================================

    // LEDs on Port C (PC4, PC5)
    GPIO_InitStruct.Pin = LED1_PIN | LED2_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOC, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);

    // LEDs on Port B (PB0, PB1)
    GPIO_InitStruct.Pin = LED3_PIN | LED4_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, LED3_PIN | LED4_PIN, GPIO_PIN_RESET);

    // ========================================================================
    // Power Control (PA10) - Enable 3.3V Analog
    // ========================================================================

    GPIO_InitStruct.Pin = PWR_EN_3V3A_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PWR_EN_3V3A_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PWR_EN_3V3A_PORT, PWR_EN_3V3A_PIN, GPIO_PIN_RESET); // Start disabled

    // ========================================================================
    // DAC Reset Pins - Output Push-Pull, Active LOW
    // ========================================================================

    // DAC0 Reset (PE10)
    GPIO_InitStruct.Pin = DAC0_RST_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DAC0_RST_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_RESET); // Start in reset

    // DAC1 Reset (PE3)
    GPIO_InitStruct.Pin = DAC1_RST_PIN;
    HAL_GPIO_Init(DAC1_RST_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC1_RST_PORT, DAC1_RST_PIN, GPIO_PIN_RESET);

    // DAC2 Reset (PC10)
    GPIO_InitStruct.Pin = DAC2_RST_PIN;
    HAL_GPIO_Init(DAC2_RST_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC2_RST_PORT, DAC2_RST_PIN, GPIO_PIN_RESET);

    // DAC3 Reset (PB15)
    GPIO_InitStruct.Pin = DAC3_RST_PIN;
    HAL_GPIO_Init(DAC3_RST_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC3_RST_PORT, DAC3_RST_PIN, GPIO_PIN_RESET);

    // ========================================================================
    // DAC Mute Input Pins - Output Push-Pull, Active LOW (LOW=muted)
    // ========================================================================

    // DAC0 AMUTEI (PE11)
    GPIO_InitStruct.Pin = DAC0_AMUTEI_PIN;
    HAL_GPIO_Init(DAC0_AMUTEI_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC0_AMUTEI_PORT, DAC0_AMUTEI_PIN, GPIO_PIN_SET); // Unmuted

    // DAC1 AMUTEI (PE1)
    GPIO_InitStruct.Pin = DAC1_AMUTEI_PIN;
    HAL_GPIO_Init(DAC1_AMUTEI_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC1_AMUTEI_PORT, DAC1_AMUTEI_PIN, GPIO_PIN_SET);

    // DAC2 AMUTEI (PC11)
    GPIO_InitStruct.Pin = DAC2_AMUTEI_PIN;
    HAL_GPIO_Init(DAC2_AMUTEI_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC2_AMUTEI_PORT, DAC2_AMUTEI_PIN, GPIO_PIN_SET);

    // DAC3 AMUTEI (PB14)
    GPIO_InitStruct.Pin = DAC3_AMUTEI_PIN;
    HAL_GPIO_Init(DAC3_AMUTEI_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DAC3_AMUTEI_PORT, DAC3_AMUTEI_PIN, GPIO_PIN_SET);

    // ========================================================================
    // DAC Mute Output Pins - Input (status from DAC)
    // ========================================================================

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    // DAC0 AMUTEO (PE12)
    GPIO_InitStruct.Pin = DAC0_AMUTEO_PIN;
    HAL_GPIO_Init(DAC0_AMUTEO_PORT, &GPIO_InitStruct);

    // DAC1 AMUTEO (PE0)
    GPIO_InitStruct.Pin = DAC1_AMUTEO_PIN;
    HAL_GPIO_Init(DAC1_AMUTEO_PORT, &GPIO_InitStruct);

    // DAC2 AMUTEO (PC12)
    GPIO_InitStruct.Pin = DAC2_AMUTEO_PIN;
    HAL_GPIO_Init(DAC2_AMUTEO_PORT, &GPIO_InitStruct);

    // DAC3 AMUTEO (PB13)
    GPIO_InitStruct.Pin = DAC3_AMUTEO_PIN;
    HAL_GPIO_Init(DAC3_AMUTEO_PORT, &GPIO_InitStruct);

    // ========================================================================
    // ADC Control Pins (PCM1802 - Extended in v2.0)
    // ========================================================================

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    // ADC Power Down (PA7) - HIGH = normal operation
    GPIO_InitStruct.Pin = ADC_PDWN_PIN;
    HAL_GPIO_Init(ADC_PDWN_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ADC_PDWN_PORT, ADC_PDWN_PIN, GPIO_PIN_RESET); // Start powered down

    // ADC OSR (PA5) - HIGH = 128x oversampling
    GPIO_InitStruct.Pin = ADC_OSR_PIN;
    HAL_GPIO_Init(ADC_OSR_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ADC_OSR_PORT, ADC_OSR_PIN, GPIO_PIN_SET); // 128x oversampling

    // ADC HPF Bypass (PA6) - LOW = HPF active
    GPIO_InitStruct.Pin = ADC_BYPASS_PIN;
    HAL_GPIO_Init(ADC_BYPASS_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ADC_BYPASS_PORT, ADC_BYPASS_PIN, GPIO_PIN_RESET); // HPF active

    // ADC FSYNC (PA4) - Frame sync input
    GPIO_InitStruct.Pin = ADC_FSYNC_PIN;
    HAL_GPIO_Init(ADC_FSYNC_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ADC_FSYNC_PORT, ADC_FSYNC_PIN, GPIO_PIN_RESET);

    // ADC Format FMT0 (PC7), FMT1 (PC6) - Set to I2S (00)
    GPIO_InitStruct.Pin = ADC_FMT0_PIN | ADC_FMT1_PIN;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ADC_FMT0_PORT, ADC_FMT0_PIN, GPIO_PIN_RESET); // FMT0 = 0
    HAL_GPIO_WritePin(ADC_FMT1_PORT, ADC_FMT1_PIN, GPIO_PIN_RESET); // FMT1 = 0 -> I2S

    // ADC Mode MODE0 (PD14), MODE1 (PD15) - Set to Slave (00)
    GPIO_InitStruct.Pin = ADC_MODE0_PIN | ADC_MODE1_PIN;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ADC_MODE0_PORT, ADC_MODE0_PIN, GPIO_PIN_RESET); // MODE0 = 0
    HAL_GPIO_WritePin(ADC_MODE1_PORT, ADC_MODE1_PIN, GPIO_PIN_RESET); // MODE1 = 0 -> Slave

    // ========================================================================
    // EEPROM Write Protect (PB5)
    // ========================================================================

    GPIO_InitStruct.Pin = EEPROM_WP_PIN;
    HAL_GPIO_Init(EEPROM_WP_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN, GPIO_PIN_SET); // Start protected

    // ========================================================================
    // Test Points (PE13, PE14, PA8) - Output for debug
    // ========================================================================

    GPIO_InitStruct.Pin = TP1_PIN | TP2_PIN;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOE, TP1_PIN | TP2_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = TP3_PIN;
    HAL_GPIO_Init(TP3_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(TP3_PORT, TP3_PIN, GPIO_PIN_RESET);
}

/**
 * @brief Initialize SAI1 GPIO (Audio Output to DAC0, DAC1)
 *
 * SAI1_MCLK: PE2 (shared master clock to all audio chips)
 * SAI1_A (DAC0): PE7 (FS), PE8 (SCK), PE9 (SD)
 * SAI1_B (DAC1): PE4 (FS), PE5 (SCK), PE6 (SD)
 */
void GPIO_SAI1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();

    // SAI1_MCLK (PE2) - Master clock shared by all audio chips
    GPIO_InitStruct.Pin = SAI1_MCLK_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = SAI1_MCLK_AF;
    HAL_GPIO_Init(SAI1_MCLK_PORT, &GPIO_InitStruct);

    // SAI1_A (DAC0): PE7 (FS), PE8 (SCK), PE9 (SD)
    GPIO_InitStruct.Pin = SAI1_FS_A_PIN | SAI1_SCK_A_PIN | SAI1_SD_A_PIN;
    GPIO_InitStruct.Alternate = GPIO_AF6_SAI1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // SAI1_B (DAC1): PE4 (FS), PE5 (SCK), PE6 (SD)
    GPIO_InitStruct.Pin = SAI1_FS_B_PIN | SAI1_SCK_B_PIN | SAI1_SD_B_PIN;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

/**
 * @brief Initialize SAI2 GPIO (Audio Output to DAC2, DAC3)
 *
 * SAI2_A (DAC2): PD13 (FS), PD12 (SCK), PD11 (SD)
 * SAI2_B (DAC3): PD10 (FS), PD9 (SCK), PD8 (SD)
 */
void GPIO_SAI2_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();

    // SAI2_A (DAC2): PD13 (FS), PD12 (SCK), PD11 (SD)
    GPIO_InitStruct.Pin = SAI2_FS_A_PIN | SAI2_SCK_A_PIN | SAI2_SD_A_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = SAI2_A_AF;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    // SAI2_B (DAC3): PD10 (FS), PD9 (SCK), PD8 (SD)
    GPIO_InitStruct.Pin = SAI2_FS_B_PIN | SAI2_SCK_B_PIN | SAI2_SD_B_PIN;
    GPIO_InitStruct.Alternate = SAI2_B_AF;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}

/**
 * @brief Initialize SAI4 GPIO (Audio Input from ADC)
 *
 * SAI4_B (ADC): PC0 (FS), PC1 (SCK), PB11 (SD)
 */
void GPIO_SAI4_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // SAI4_FS_B (PC0), SAI4_SCK_B (PC1)
    GPIO_InitStruct.Pin = SAI4_FS_B_PIN | SAI4_SCK_B_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = SAI4_B_AF;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // SAI4_SD_B (PB11) - Data input
    GPIO_InitStruct.Pin = SAI4_SD_B_PIN;
    GPIO_InitStruct.Alternate = SAI4_B_AF;
    HAL_GPIO_Init(SAI4_SD_B_PORT, &GPIO_InitStruct);
}

/**
 * @brief Initialize I2C1 GPIO (DAC configuration bus)
 *
 * PB8 (SCL), PB9 (SDA) - Open Drain with external 5.1kΩ pullups
 */
void GPIO_I2C1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = I2C1_SCL_PIN | I2C1_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;  // External pullups
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = I2C1_AF;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/**
 * @brief Initialize I2C3 GPIO (EEPROM bus)
 *
 * PB6 (SCL), PB7 (SDA) - Open Drain with external 5.1kΩ pullups
 */
void GPIO_I2C3_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = I2C3_SCL_PIN | I2C3_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;  // External pullups
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = I2C3_AF;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/**
 * @brief Initialize UART4 GPIO (ESP32-C3 communication)
 *
 * PA0 (TX), PA1 (RX) - Direct connection (no series resistors)
 */
void GPIO_UART4_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    // UART4_TX (PA0)
    GPIO_InitStruct.Pin = UART4_TX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = UART4_AF;
    HAL_GPIO_Init(UART4_TX_PORT, &GPIO_InitStruct);

    // UART4_RX (PA1)
    GPIO_InitStruct.Pin = UART4_RX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Alternate = UART4_AF;
    HAL_GPIO_Init(UART4_RX_PORT, &GPIO_InitStruct);
}

/* ============================================================================
 * DAC Control Functions
 * ============================================================================ */

/**
 * @brief Release all DACs from reset sequentially
 *
 * Releases DACs one at a time with 1ms delay between each.
 * Must be called AFTER MCLK is running.
 */
void DAC_Release_Reset(void)
{
    HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(DAC1_RST_PORT, DAC1_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(DAC2_RST_PORT, DAC2_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(DAC3_RST_PORT, DAC3_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(10); // Wait for DACs to initialize
}

/**
 * @brief Put all DACs in reset
 */
void DAC_Assert_Reset(void)
{
    HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DAC1_RST_PORT, DAC1_RST_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DAC2_RST_PORT, DAC2_RST_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DAC3_RST_PORT, DAC3_RST_PIN, GPIO_PIN_RESET);
}

/**
 * @brief Release specific DAC from reset
 * @param dac_num DAC number (0-3)
 */
void DAC_Release_Reset_Single(uint8_t dac_num)
{
    switch (dac_num) {
        case 0:
            HAL_GPIO_WritePin(DAC0_RST_PORT, DAC0_RST_PIN, GPIO_PIN_SET);
            break;
        case 1:
            HAL_GPIO_WritePin(DAC1_RST_PORT, DAC1_RST_PIN, GPIO_PIN_SET);
            break;
        case 2:
            HAL_GPIO_WritePin(DAC2_RST_PORT, DAC2_RST_PIN, GPIO_PIN_SET);
            break;
        case 3:
            HAL_GPIO_WritePin(DAC3_RST_PORT, DAC3_RST_PIN, GPIO_PIN_SET);
            break;
    }
}

/**
 * @brief Mute/unmute all DACs
 * @param mute 1 = mute, 0 = unmute
 */
void DAC_Set_Mute(uint8_t mute)
{
    GPIO_PinState state = mute ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(DAC0_AMUTEI_PORT, DAC0_AMUTEI_PIN, state);
    HAL_GPIO_WritePin(DAC1_AMUTEI_PORT, DAC1_AMUTEI_PIN, state);
    HAL_GPIO_WritePin(DAC2_AMUTEI_PORT, DAC2_AMUTEI_PIN, state);
    HAL_GPIO_WritePin(DAC3_AMUTEI_PORT, DAC3_AMUTEI_PIN, state);
}

/**
 * @brief Mute/unmute specific DAC
 * @param dac_num DAC number (0-3)
 * @param mute 1 = mute, 0 = unmute
 */
void DAC_Set_Mute_Single(uint8_t dac_num, uint8_t mute)
{
    GPIO_PinState state = mute ? GPIO_PIN_RESET : GPIO_PIN_SET;

    switch (dac_num) {
        case 0:
            HAL_GPIO_WritePin(DAC0_AMUTEI_PORT, DAC0_AMUTEI_PIN, state);
            break;
        case 1:
            HAL_GPIO_WritePin(DAC1_AMUTEI_PORT, DAC1_AMUTEI_PIN, state);
            break;
        case 2:
            HAL_GPIO_WritePin(DAC2_AMUTEI_PORT, DAC2_AMUTEI_PIN, state);
            break;
        case 3:
            HAL_GPIO_WritePin(DAC3_AMUTEI_PORT, DAC3_AMUTEI_PIN, state);
            break;
    }
}

/**
 * @brief Read DAC mute output status
 * @param dac_num DAC number (0-3)
 * @return 1 if muted, 0 if unmuted
 */
uint8_t DAC_Get_Mute_Status(uint8_t dac_num)
{
    GPIO_PinState state = GPIO_PIN_RESET;

    switch (dac_num) {
        case 0:
            state = HAL_GPIO_ReadPin(DAC0_AMUTEO_PORT, DAC0_AMUTEO_PIN);
            break;
        case 1:
            state = HAL_GPIO_ReadPin(DAC1_AMUTEO_PORT, DAC1_AMUTEO_PIN);
            break;
        case 2:
            state = HAL_GPIO_ReadPin(DAC2_AMUTEO_PORT, DAC2_AMUTEO_PIN);
            break;
        case 3:
            state = HAL_GPIO_ReadPin(DAC3_AMUTEO_PORT, DAC3_AMUTEO_PIN);
            break;
    }

    return (state == GPIO_PIN_RESET) ? 1 : 0;
}

/* ============================================================================
 * ADC Control Functions
 * ============================================================================ */

/**
 * @brief Power on the ADC (PCM1802)
 */
void ADC_PowerOn(void)
{
    HAL_GPIO_WritePin(ADC_PDWN_PORT, ADC_PDWN_PIN, GPIO_PIN_SET);
}

/**
 * @brief Power off the ADC (PCM1802)
 */
void ADC_PowerOff(void)
{
    HAL_GPIO_WritePin(ADC_PDWN_PORT, ADC_PDWN_PIN, GPIO_PIN_RESET);
}

/**
 * @brief Set ADC oversampling ratio
 * @param high_osr 1 = 128x (for fs < 50kHz), 0 = 64x (for fs > 50kHz)
 */
void ADC_SetOSR(uint8_t high_osr)
{
    HAL_GPIO_WritePin(ADC_OSR_PORT, ADC_OSR_PIN,
                      high_osr ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief Set ADC HPF bypass
 * @param bypass 1 = bypass HPF, 0 = HPF active
 */
void ADC_SetHPFBypass(uint8_t bypass)
{
    HAL_GPIO_WritePin(ADC_BYPASS_PORT, ADC_BYPASS_PIN,
                      bypass ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief Set ADC audio format
 * @param format 0=I2S, 1=Left-Just, 2=Right-Just 24-bit, 3=Right-Just 16-bit
 */
void ADC_SetFormat(uint8_t format)
{
    HAL_GPIO_WritePin(ADC_FMT0_PORT, ADC_FMT0_PIN,
                      (format & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ADC_FMT1_PORT, ADC_FMT1_PIN,
                      (format & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief Set ADC operating mode
 * @param mode 0=Slave, 1=Master 256fs, 2=Master 384fs, 3=Master 512fs
 */
void ADC_SetMode(uint8_t mode)
{
    HAL_GPIO_WritePin(ADC_MODE0_PORT, ADC_MODE0_PIN,
                      (mode & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ADC_MODE1_PORT, ADC_MODE1_PIN,
                      (mode & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ============================================================================
 * EEPROM Control Functions
 * ============================================================================ */

/**
 * @brief Enable EEPROM write protection
 */
void EEPROM_WriteProtect_Enable(void)
{
    HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN, GPIO_PIN_SET);
}

/**
 * @brief Disable EEPROM write protection (allow writes)
 */
void EEPROM_WriteProtect_Disable(void)
{
    HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN, GPIO_PIN_RESET);
}

/* ============================================================================
 * LED Control Functions
 * ============================================================================ */

/**
 * @brief Set LED state
 * @param led_num LED number (1-4)
 * @param state 1 = on, 0 = off
 */
void LED_Set(uint8_t led_num, uint8_t state)
{
    GPIO_PinState pin_state = state ? GPIO_PIN_SET : GPIO_PIN_RESET;

    switch (led_num) {
        case 1:
            HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, pin_state);
            break;
        case 2:
            HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, pin_state);
            break;
        case 3:
            HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, pin_state);
            break;
        case 4:
            HAL_GPIO_WritePin(LED4_PORT, LED4_PIN, pin_state);
            break;
    }
}

/**
 * @brief Toggle LED state
 * @param led_num LED number (1-4)
 */
void LED_Toggle(uint8_t led_num)
{
    switch (led_num) {
        case 1:
            HAL_GPIO_TogglePin(LED1_PORT, LED1_PIN);
            break;
        case 2:
            HAL_GPIO_TogglePin(LED2_PORT, LED2_PIN);
            break;
        case 3:
            HAL_GPIO_TogglePin(LED3_PORT, LED3_PIN);
            break;
        case 4:
            HAL_GPIO_TogglePin(LED4_PORT, LED4_PIN);
            break;
    }
}

/**
 * @brief Set all LEDs
 * @param mask Bit mask for LEDs (bit 0=LED1, bit 1=LED2, etc.)
 */
void LED_SetAll(uint8_t mask)
{
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, (mask & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, (mask & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, (mask & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED4_PORT, LED4_PIN, (mask & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ============================================================================
 * Power Sequence Functions
 * ============================================================================ */

/**
 * @brief Enable 3.3V analog power
 */
void PWR_Enable_3V3A(void)
{
    HAL_GPIO_WritePin(PWR_EN_3V3A_PORT, PWR_EN_3V3A_PIN, GPIO_PIN_SET);
}

/**
 * @brief Disable 3.3V analog power
 */
void PWR_Disable_3V3A(void)
{
    HAL_GPIO_WritePin(PWR_EN_3V3A_PORT, PWR_EN_3V3A_PIN, GPIO_PIN_RESET);
}
