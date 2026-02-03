/**
 * @file gpio.h
 * @brief GPIO function declarations for Domosonica DS9 v2.0
 *
 * Hardware: STM32H743VIT6 LQFP100
 * Version: 2.0 - 32-channel beamforming system
 */

#ifndef __GPIO_H
#define __GPIO_H

#include <stdint.h>

/* ============================================================================
 * GPIO Initialization Functions
 * ============================================================================ */

/**
 * @brief Initialize all GPIO pins
 */
void GPIO_Init_All(void);

/**
 * @brief Initialize SAI1 GPIO (DAC0, DAC1)
 */
void GPIO_SAI1_Init(void);

/**
 * @brief Initialize SAI2 GPIO (DAC2, DAC3)
 */
void GPIO_SAI2_Init(void);

/**
 * @brief Initialize SAI4 GPIO (ADC)
 */
void GPIO_SAI4_Init(void);

/**
 * @brief Initialize I2C1 GPIO (DAC bus)
 */
void GPIO_I2C1_Init(void);

/**
 * @brief Initialize I2C3 GPIO (EEPROM bus)
 */
void GPIO_I2C3_Init(void);

/**
 * @brief Initialize UART4 GPIO (ESP32 communication)
 */
void GPIO_UART4_Init(void);

/* ============================================================================
 * DAC Control Functions
 * ============================================================================ */

/**
 * @brief Release all DACs from reset sequentially
 */
void DAC_Release_Reset(void);

/**
 * @brief Put all DACs in reset
 */
void DAC_Assert_Reset(void);

/**
 * @brief Release specific DAC from reset
 * @param dac_num DAC number (0-3)
 */
void DAC_Release_Reset_Single(uint8_t dac_num);

/**
 * @brief Mute/unmute all DACs
 * @param mute 1 = mute, 0 = unmute
 */
void DAC_Set_Mute(uint8_t mute);

/**
 * @brief Mute/unmute specific DAC
 * @param dac_num DAC number (0-3)
 * @param mute 1 = mute, 0 = unmute
 */
void DAC_Set_Mute_Single(uint8_t dac_num, uint8_t mute);

/**
 * @brief Read DAC mute output status
 * @param dac_num DAC number (0-3)
 * @return 1 if muted, 0 if unmuted
 */
uint8_t DAC_Get_Mute_Status(uint8_t dac_num);

/* ============================================================================
 * ADC Control Functions
 * ============================================================================ */

/**
 * @brief Power on the ADC (PCM1802)
 */
void ADC_PowerOn(void);

/**
 * @brief Power off the ADC (PCM1802)
 */
void ADC_PowerOff(void);

/**
 * @brief Set ADC oversampling ratio
 * @param high_osr 1 = 128x (for fs < 50kHz), 0 = 64x (for fs > 50kHz)
 */
void ADC_SetOSR(uint8_t high_osr);

/**
 * @brief Set ADC HPF bypass
 * @param bypass 1 = bypass HPF, 0 = HPF active
 */
void ADC_SetHPFBypass(uint8_t bypass);

/**
 * @brief Set ADC audio format
 * @param format 0=I2S, 1=Left-Just, 2=Right-Just 24-bit, 3=Right-Just 16-bit
 */
void ADC_SetFormat(uint8_t format);

/**
 * @brief Set ADC operating mode
 * @param mode 0=Slave, 1=Master 256fs, 2=Master 384fs, 3=Master 512fs
 */
void ADC_SetMode(uint8_t mode);

/* ADC Format constants */
#define ADC_FORMAT_I2S              0
#define ADC_FORMAT_LEFT_JUST        1
#define ADC_FORMAT_RIGHT_JUST_24    2
#define ADC_FORMAT_RIGHT_JUST_16    3

/* ADC Mode constants */
#define ADC_MODE_SLAVE              0
#define ADC_MODE_MASTER_256FS       1
#define ADC_MODE_MASTER_384FS       2
#define ADC_MODE_MASTER_512FS       3

/* ============================================================================
 * EEPROM Control Functions
 * ============================================================================ */

/**
 * @brief Enable EEPROM write protection
 */
void EEPROM_WriteProtect_Enable(void);

/**
 * @brief Disable EEPROM write protection (allow writes)
 */
void EEPROM_WriteProtect_Disable(void);

/* ============================================================================
 * LED Control Functions
 * ============================================================================ */

/**
 * @brief Set LED state
 * @param led_num LED number (1-4)
 * @param state 1 = on, 0 = off
 */
void LED_Set(uint8_t led_num, uint8_t state);

/**
 * @brief Toggle LED state
 * @param led_num LED number (1-4)
 */
void LED_Toggle(uint8_t led_num);

/**
 * @brief Set all LEDs
 * @param mask Bit mask for LEDs (bit 0=LED1, bit 1=LED2, etc.)
 */
void LED_SetAll(uint8_t mask);

/* ============================================================================
 * Power Control Functions
 * ============================================================================ */

/**
 * @brief Enable 3.3V analog power
 */
void PWR_Enable_3V3A(void);

/**
 * @brief Disable 3.3V analog power
 */
void PWR_Disable_3V3A(void);

#endif /* __GPIO_H */
