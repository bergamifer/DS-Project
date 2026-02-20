/**
 * @file pcm1690.c
 * @brief PCM1690 8-channel DAC driver implementation
 *
 * Uses HAL I2C directly - no dependency on i2c.c
 * All addresses are 8-bit (include R/W bit) as per pin_definitions.h
 */

#include "pcm1690.h"

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

static HAL_StatusTypeDef PCM1690_WriteReg_Internal(I2C_HandleTypeDef *hi2c,
                                                    uint8_t dev_addr,
                                                    uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return HAL_I2C_Master_Transmit(hi2c, dev_addr, buf, 2, 100);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

PCM1690_Status PCM1690_Init(I2C_HandleTypeDef *hi2c, uint8_t dev_addr,
                             const PCM1690_Pins *pins)
{
    PCM1690_Status status;

    // Step 1: Assert RST LOW (hold in reset)
    if (pins != NULL) {
        HAL_GPIO_WritePin(pins->rst_port, pins->rst_pin, GPIO_PIN_RESET);
        // AMUTEI LOW = muted during config
        HAL_GPIO_WritePin(pins->amutei_port, pins->amutei_pin, GPIO_PIN_RESET);
    }

    // Step 2: MCLK must already be running (caller's responsibility)
    // Step 3: Wait 1ms with RST held LOW
    HAL_Delay(1);

    // Step 4: Release RST (HIGH)
    if (pins != NULL) {
        HAL_GPIO_WritePin(pins->rst_port, pins->rst_pin, GPIO_PIN_SET);
    }

    // Step 5: Wait 10ms for internal reset + PLL lock
    HAL_Delay(10);

    // Step 6: Check I2C communication (3 retries)
    for (uint8_t retry = 0; retry < 3; retry++) {
        if (PCM1690_IsReady(hi2c, dev_addr)) break;
        HAL_Delay(5);
        if (retry == 2) return PCM1690_ERR_NOT_FOUND;
    }

    // Step 7: Configure TDM8 mode
    status = PCM1690_ConfigTDM(hi2c, dev_addr);
    if (status != PCM1690_OK) return status;

    // Step 8: Verify configuration
    status = PCM1690_Verify(hi2c, dev_addr);
    if (status != PCM1690_OK) return status;

    // Step 9: Unmute - soft mute register first, then AMUTEI pin
    status = PCM1690_SoftMute(hi2c, dev_addr, PCM1690_MUTE_NONE);
    if (status != PCM1690_OK) return status;

    if (pins != NULL) {
        HAL_GPIO_WritePin(pins->amutei_port, pins->amutei_pin, GPIO_PIN_SET);
    }

    return PCM1690_OK;
}

PCM1690_Status PCM1690_ConfigTDM(I2C_HandleTypeDef *hi2c, uint8_t dev_addr)
{
    HAL_StatusTypeDef status;

    // Reg 64: Auto sample rate, normal operation
    status = PCM1690_WriteReg_Internal(hi2c, dev_addr, PCM1690_REG_SYSTEM,
                                        PCM1690_SRDA_AUTO);
    if (status != HAL_OK) return PCM1690_ERR_I2C;

    // Reg 65: TDM I2S mode (FMTDA[3:0] = 0110)
    status = PCM1690_WriteReg_Internal(hi2c, dev_addr, PCM1690_REG_FORMAT,
                                        PCM1690_FMT_TDM_I2S);
    if (status != HAL_OK) return PCM1690_ERR_I2C;

    // Reg 66: All DACs enabled, sharp roll-off
    status = PCM1690_WriteReg_Internal(hi2c, dev_addr, PCM1690_REG_OPERATION,
                                        PCM1690_ALL_DAC_ON);
    if (status != HAL_OK) return PCM1690_ERR_I2C;

    // Reg 70: De-emphasis off, fine attenuation (0.5dB steps)
    status = PCM1690_WriteReg_Internal(hi2c, dev_addr, PCM1690_REG_DEEMPH, 0x00);
    if (status != HAL_OK) return PCM1690_ERR_I2C;

    // Reg 72-79: All 8 channels at 0dB (no attenuation)
    for (uint8_t ch = 0; ch < 8; ch++) {
        status = PCM1690_WriteReg_Internal(hi2c, dev_addr,
                                            PCM1690_REG_ATTEN_CH1 + ch,
                                            PCM1690_ATTEN_0DB);
        if (status != HAL_OK) return PCM1690_ERR_I2C;
    }

    HAL_Delay(10);  // Wait for config to take effect
    return PCM1690_OK;
}

uint8_t PCM1690_IsReady(I2C_HandleTypeDef *hi2c, uint8_t dev_addr)
{
    return (HAL_I2C_IsDeviceReady(hi2c, dev_addr, 1, 10) == HAL_OK) ? 1 : 0;
}

PCM1690_Status PCM1690_SetAttenuation(I2C_HandleTypeDef *hi2c, uint8_t dev_addr,
                                       uint8_t channel, uint8_t atten)
{
    if (channel < 1 || channel > 8) return PCM1690_ERR_I2C;

    uint8_t reg = PCM1690_REG_ATTEN_CH1 + (channel - 1);
    HAL_StatusTypeDef status = PCM1690_WriteReg_Internal(hi2c, dev_addr, reg, atten);
    return (status == HAL_OK) ? PCM1690_OK : PCM1690_ERR_I2C;
}

PCM1690_Status PCM1690_SoftMute(I2C_HandleTypeDef *hi2c, uint8_t dev_addr,
                                 uint8_t mute_mask)
{
    HAL_StatusTypeDef status = PCM1690_WriteReg_Internal(hi2c, dev_addr,
                                                          PCM1690_REG_MUTE,
                                                          mute_mask);
    return (status == HAL_OK) ? PCM1690_OK : PCM1690_ERR_I2C;
}

PCM1690_Status PCM1690_ReadReg(I2C_HandleTypeDef *hi2c, uint8_t dev_addr,
                                uint8_t reg, uint8_t *data)
{
    HAL_StatusTypeDef status;

    // Write register address
    status = HAL_I2C_Master_Transmit(hi2c, dev_addr, &reg, 1, 100);
    if (status != HAL_OK) return PCM1690_ERR_I2C;

    // Read register value
    status = HAL_I2C_Master_Receive(hi2c, dev_addr | 0x01, data, 1, 100);
    return (status == HAL_OK) ? PCM1690_OK : PCM1690_ERR_I2C;
}

PCM1690_Status PCM1690_Verify(I2C_HandleTypeDef *hi2c, uint8_t dev_addr)
{
    uint8_t val;
    PCM1690_Status status;

    // Verify format register (should be TDM I2S = 0x06)
    status = PCM1690_ReadReg(hi2c, dev_addr, PCM1690_REG_FORMAT, &val);
    if (status != PCM1690_OK) return status;
    if ((val & 0x0F) != PCM1690_FMT_TDM_I2S) return PCM1690_ERR_VERIFY;

    // Verify operation register (should be all DACs enabled = 0x00 in upper nibble)
    status = PCM1690_ReadReg(hi2c, dev_addr, PCM1690_REG_OPERATION, &val);
    if (status != PCM1690_OK) return status;
    if ((val & 0xF0) != 0x00) return PCM1690_ERR_VERIFY;

    return PCM1690_OK;
}
