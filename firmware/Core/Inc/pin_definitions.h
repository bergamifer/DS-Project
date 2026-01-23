/**
 * @file pin_definitions.h
 * @brief Pin definitions for Domosonica DS - STM32H743VIT6
 *
 * Hardware: WeAct MiniSTM32H7xx (STM32H743VIT6)
 * Crystal: 25 MHz HSE
 */

#ifndef __PIN_DEFINITIONS_H
#define __PIN_DEFINITIONS_H

/* ============================================================================
 * SAI1 - Audio Output (DACs)
 * ============================================================================ */

// SAI1_A (DAC0 - Master Async)
#define SAI1_MCLK_PIN           GPIO_PIN_2
#define SAI1_MCLK_PORT          GPIOE
#define SAI1_FS_A_PIN           GPIO_PIN_4
#define SAI1_FS_A_PORT          GPIOE
#define SAI1_SCK_A_PIN          GPIO_PIN_5
#define SAI1_SCK_A_PORT         GPIOE
#define SAI1_SD_A_PIN           GPIO_PIN_6
#define SAI1_SD_A_PORT          GPIOE

// SAI1_B (DAC1 - Master Sync)
#define SAI1_SD_B_PIN           GPIO_PIN_7
#define SAI1_SD_B_PORT          GPIOE
#define SAI1_SCK_B_PIN          GPIO_PIN_8
#define SAI1_SCK_B_PORT         GPIOE
#define SAI1_FS_B_PIN           GPIO_PIN_9
#define SAI1_FS_B_PORT          GPIOE

/* ============================================================================
 * SAI4 - Audio Input (ADC)
 * ============================================================================ */

#define SAI4_FS_B_PIN           GPIO_PIN_0
#define SAI4_FS_B_PORT          GPIOC
#define SAI4_SCK_B_PIN          GPIO_PIN_1
#define SAI4_SCK_B_PORT         GPIOC
#define SAI4_SD_B_PIN           GPIO_PIN_11
#define SAI4_SD_B_PORT          GPIOB

/* ============================================================================
 * I2C1 - Control Interface (DACs/ADC configuration)
 * ============================================================================ */

#define I2C1_SCL_PIN            GPIO_PIN_8
#define I2C1_SCL_PORT           GPIOB
#define I2C1_SDA_PIN            GPIO_PIN_9
#define I2C1_SDA_PORT           GPIOB

// I2C Addresses
#define PCM1690_DAC0_ADDR       0x4C
#define PCM1690_DAC1_ADDR       0x4E
#define PCM1802_ADC_ADDR        0x4D

/* ============================================================================
 * UART7 - ESP32 Communication
 * ============================================================================ */

#define UART7_RX_PIN            GPIO_PIN_8
#define UART7_RX_PORT           GPIOA
#define UART7_TX_PIN            GPIO_PIN_15
#define UART7_TX_PORT           GPIOA

/* ============================================================================
 * DAC Control Pins
 * ============================================================================ */

// DAC0 (PCM1690 #0)
#define DAC0_RST_PIN            GPIO_PIN_6
#define DAC0_RST_PORT           GPIOA
#define DAC0_AMUTEI_PIN         GPIO_PIN_1
#define DAC0_AMUTEI_PORT        GPIOD
#define DAC0_AMUTEO_PIN         GPIO_PIN_3
#define DAC0_AMUTEO_PORT        GPIOD

// DAC1 (PCM1690 #1)
#define DAC1_RST_PIN            GPIO_PIN_7
#define DAC1_RST_PORT           GPIOA
#define DAC1_AMUTEI_PIN         GPIO_PIN_1
#define DAC1_AMUTEI_PORT        GPIOB
#define DAC1_AMUTEO_PIN         GPIO_PIN_5
#define DAC1_AMUTEO_PORT        GPIOC

/* ============================================================================
 * ADC Control Pins (PCM1802)
 * ============================================================================ */

#define ADC_PDWN_PIN            GPIO_PIN_1
#define ADC_PDWN_PORT           GPIOA
#define ADC_OSR_PIN             GPIO_PIN_0
#define ADC_OSR_PORT            GPIOB
#define ADC_BYPASS_PIN          GPIO_PIN_3
#define ADC_BYPASS_PORT         GPIOC

/* ============================================================================
 * Status LED
 * ============================================================================ */

#define LED_STATUS_PIN          GPIO_PIN_5
#define LED_STATUS_PORT         GPIOB

/* ============================================================================
 * SDMMC - MicroSD Card
 * ============================================================================ */

#define SDMMC_D0_PIN            GPIO_PIN_8
#define SDMMC_D0_PORT           GPIOC
#define SDMMC_D1_PIN            GPIO_PIN_9
#define SDMMC_D1_PORT           GPIOC
#define SDMMC_D2_PIN            GPIO_PIN_10
#define SDMMC_D2_PORT           GPIOC
#define SDMMC_D3_PIN            GPIO_PIN_11
#define SDMMC_D3_PORT           GPIOC
#define SDMMC_CK_PIN            GPIO_PIN_12
#define SDMMC_CK_PORT           GPIOC
#define SDMMC_CMD_PIN           GPIO_PIN_2
#define SDMMC_CMD_PORT          GPIOD

/* ============================================================================
 * LCD Interface (GPIO bitbanged) - Optional
 * ============================================================================ */

#define LCD_LED_PIN             GPIO_PIN_10
#define LCD_LED_PORT            GPIOE
#define LCD_CS_PIN              GPIO_PIN_11
#define LCD_CS_PORT             GPIOE
#define LCD_SCL_PIN             GPIO_PIN_12
#define LCD_SCL_PORT            GPIOE
#define LCD_WR_RS_PIN           GPIO_PIN_13
#define LCD_WR_RS_PORT          GPIOE
#define LCD_SDA_PIN             GPIO_PIN_14
#define LCD_SDA_PORT            GPIOE

#endif /* __PIN_DEFINITIONS_H */
