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
#define ADC_BYPASS_Pin GPIO_PIN_3
#define ADC_BYPASS_GPIO_Port GPIOC
#define ADC_PDWN_Pin GPIO_PIN_1
#define ADC_PDWN_GPIO_Port GPIOA
#define DAC0_RST_Pin GPIO_PIN_6
#define DAC0_RST_GPIO_Port GPIOA
#define DAC1_RST_Pin GPIO_PIN_7
#define DAC1_RST_GPIO_Port GPIOA
#define DAC1_AMUTEO_Pin GPIO_PIN_5
#define DAC1_AMUTEO_GPIO_Port GPIOC
#define ADC_OSR_Pin GPIO_PIN_0
#define ADC_OSR_GPIO_Port GPIOB
#define DAC1_AMUTEI_Pin GPIO_PIN_1
#define DAC1_AMUTEI_GPIO_Port GPIOB
#define DAC0_AMUTEI_Pin GPIO_PIN_1
#define DAC0_AMUTEI_GPIO_Port GPIOD
#define DAC0_AMUTEO_Pin GPIO_PIN_3
#define DAC0_AMUTEO_GPIO_Port GPIOD
#define LED_STATUS_Pin GPIO_PIN_5
#define LED_STATUS_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
