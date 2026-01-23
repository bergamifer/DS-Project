/**
 * @file i2c.c
 * @brief I2C1 configuration for DAC/ADC control
 *
 * I2C1: PB8 (SCL), PB9 (SDA)
 * Speed: 100 kHz (Standard mode)
 * External pullups: 5.1k to 3.3V
 */

#include "main.h"

I2C_HandleTypeDef hi2c1;

/**
 * @brief Initialize I2C1 peripheral
 *
 * Timing calculated for 100 kHz @ 120 MHz PCLK1:
 *   PRESC=11, SCLDEL=4, SDADEL=2, SCLH=185, SCLL=235
 */
void I2C1_Init(void)
{
    // Enable I2C1 clock
    __HAL_RCC_I2C1_CLK_ENABLE();

    // Initialize GPIO
    GPIO_I2C1_Init();

    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x10B0DCFB;  // 100 kHz @ 120 MHz
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }

    // Configure analog filter
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    {
        Error_Handler();
    }

    // Configure digital filter
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
    {
        Error_Handler();
    }

    Debug_Print("I2C1 initialized @ 100 kHz\r\n");
}

/**
 * @brief Write register to I2C device
 * @param addr 7-bit device address
 * @param reg Register address
 * @param data Data to write
 * @return HAL_OK on success
 */
HAL_StatusTypeDef I2C_WriteReg(uint8_t addr, uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return HAL_I2C_Master_Transmit(&hi2c1, addr << 1, buf, 2, 100);
}

/**
 * @brief Read register from I2C device
 * @param addr 7-bit device address
 * @param reg Register address
 * @param data Pointer to store read data
 * @return HAL_OK on success
 */
HAL_StatusTypeDef I2C_ReadReg(uint8_t addr, uint8_t reg, uint8_t *data)
{
    HAL_StatusTypeDef status;

    status = HAL_I2C_Master_Transmit(&hi2c1, addr << 1, &reg, 1, 100);
    if (status != HAL_OK)
        return status;

    return HAL_I2C_Master_Receive(&hi2c1, addr << 1, data, 1, 100);
}

/**
 * @brief Check if I2C device is present
 * @param addr 7-bit device address
 * @return 1 if device responds, 0 otherwise
 */
uint8_t I2C_DeviceReady(uint8_t addr)
{
    return (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK) ? 1 : 0;
}
