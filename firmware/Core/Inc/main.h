/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "pin_definitions.h"
#include "gpio.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* Note: Pin definitions are now in pin_definitions.h for v2.0 hardware */
/* These CubeMX-generated defines are kept for HAL compatibility */

/* Status LEDs (v2.0: 4 LEDs) */
#define LED1_Pin GPIO_PIN_4
#define LED1_GPIO_Port GPIOC
#define LED2_Pin GPIO_PIN_5
#define LED2_GPIO_Port GPIOC
#define LED3_Pin GPIO_PIN_0
#define LED3_GPIO_Port GPIOB
#define LED4_Pin GPIO_PIN_1
#define LED4_GPIO_Port GPIOB

/* DAC Reset pins (v2.0: 4 DACs) */
#define DAC0_RST_Pin GPIO_PIN_10
#define DAC0_RST_GPIO_Port GPIOE
#define DAC1_RST_Pin GPIO_PIN_3
#define DAC1_RST_GPIO_Port GPIOE
#define DAC2_RST_Pin GPIO_PIN_10
#define DAC2_RST_GPIO_Port GPIOC
#define DAC3_RST_Pin GPIO_PIN_15
#define DAC3_RST_GPIO_Port GPIOB

/* DAC Mute pins (v2.0: 4 DACs) */
#define DAC0_AMUTEI_Pin GPIO_PIN_11
#define DAC0_AMUTEI_GPIO_Port GPIOE
#define DAC0_AMUTEO_Pin GPIO_PIN_12
#define DAC0_AMUTEO_GPIO_Port GPIOE
#define DAC1_AMUTEI_Pin GPIO_PIN_1
#define DAC1_AMUTEI_GPIO_Port GPIOE
#define DAC1_AMUTEO_Pin GPIO_PIN_0
#define DAC1_AMUTEO_GPIO_Port GPIOE
#define DAC2_AMUTEI_Pin GPIO_PIN_11
#define DAC2_AMUTEI_GPIO_Port GPIOC
#define DAC2_AMUTEO_Pin GPIO_PIN_12
#define DAC2_AMUTEO_GPIO_Port GPIOC
#define DAC3_AMUTEI_Pin GPIO_PIN_14
#define DAC3_AMUTEI_GPIO_Port GPIOB
#define DAC3_AMUTEO_Pin GPIO_PIN_13
#define DAC3_AMUTEO_GPIO_Port GPIOB

/* ADC Control pins (v2.0: 8 GPIO extended control) */
#define ADC_PDWN_Pin GPIO_PIN_7
#define ADC_PDWN_GPIO_Port GPIOA
#define ADC_OSR_Pin GPIO_PIN_5
#define ADC_OSR_GPIO_Port GPIOA
#define ADC_BYPASS_Pin GPIO_PIN_6
#define ADC_BYPASS_GPIO_Port GPIOA
#define ADC_FSYNC_Pin GPIO_PIN_4
#define ADC_FSYNC_GPIO_Port GPIOA
#define ADC_FMT0_Pin GPIO_PIN_7
#define ADC_FMT0_GPIO_Port GPIOC
#define ADC_FMT1_Pin GPIO_PIN_6
#define ADC_FMT1_GPIO_Port GPIOC
#define ADC_MODE0_Pin GPIO_PIN_14
#define ADC_MODE0_GPIO_Port GPIOD
#define ADC_MODE1_Pin GPIO_PIN_15
#define ADC_MODE1_GPIO_Port GPIOD

/* Power Control */
#define PWR_EN_3V3A_Pin GPIO_PIN_10
#define PWR_EN_3V3A_GPIO_Port GPIOA

/* EEPROM Write Protect */
#define EEPROM_WP_Pin GPIO_PIN_5
#define EEPROM_WP_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
