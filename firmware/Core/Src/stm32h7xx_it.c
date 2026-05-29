/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_it.c
  * @brief   Interrupt Service Routines.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32h7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern I2C_HandleTypeDef hi2c1;
extern DMA_HandleTypeDef hdma_sai1_a;
extern DMA_HandleTypeDef hdma_sai1_b;
extern DMA_HandleTypeDef hdma_sai2_a;
extern DMA_HandleTypeDef hdma_sai2_b;
extern DMA_HandleTypeDef hdma_sai3_b;
extern SAI_HandleTypeDef hsai_BlockA1;
extern SAI_HandleTypeDef hsai_BlockB1;
extern SAI_HandleTypeDef hsai_BlockA2;
extern SAI_HandleTypeDef hsai_BlockB2;
extern SAI_HandleTypeDef hsai_BlockB3;
extern UART_HandleTypeDef huart7;
extern UART_HandleTypeDef huart4;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  * Uses naked + assembly to capture the correct stack frame,
  * then dumps fault registers via direct UART4 polling (no HAL).
  */
__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile(
    "tst lr, #4       \n"  /* Check EXC_RETURN bit 2 */
    "ite eq            \n"
    "mrseq r0, msp     \n"  /* Was using MSP */
    "mrsne r0, psp     \n"  /* Was using PSP */
    "b HardFault_Dump  \n"
  );
}

/* Direct UART4 polling output — no HAL, no SysTick dependency */
static void fault_putchar(char c) {
  while (!(UART4->ISR & USART_ISR_TXE_TXFNF)) {}
  UART4->TDR = (uint8_t)c;
}
static void fault_print(const char *s) {
  while (*s) fault_putchar(*s++);
}
static void fault_hex32(uint32_t val) {
  const char h[] = "0123456789ABCDEF";
  fault_putchar('0'); fault_putchar('x');
  for (int i = 28; i >= 0; i -= 4)
    fault_putchar(h[(val >> i) & 0xF]);
}

void HardFault_Dump(uint32_t *stack)
{
  __disable_irq();

  /* Safety: silence all 32 PAM8302A amplifiers (PAM_SD = PD3 LOW) so a crash
   * doesn't leave the array blasting the last DMA buffer. GPIOD clk is on. */
  GPIOD->BSRR = (uint32_t)GPIO_PIN_3 << 16U;

  /* Stacked registers: R0 R1 R2 R3 R12 LR PC xPSR */
  uint32_t pc  = stack[6];
  uint32_t lr  = stack[5];
  uint32_t r0  = stack[0];

  fault_print("\r\n!!! HARDFAULT !!!\r\n");
  fault_print("PC ="); fault_hex32(pc);  fault_print("\r\n");
  fault_print("LR ="); fault_hex32(lr);  fault_print("\r\n");
  fault_print("R0 ="); fault_hex32(r0);  fault_print("\r\n");
  fault_print("HFSR="); fault_hex32(SCB->HFSR); fault_print("\r\n");
  fault_print("CFSR="); fault_hex32(SCB->CFSR); fault_print("\r\n");
  fault_print("BFAR="); fault_hex32(SCB->BFAR); fault_print("\r\n");
  fault_print("MMFAR="); fault_hex32(SCB->MMFAR); fault_print("\r\n");
  fault_print("SP ="); fault_hex32((uint32_t)stack); fault_print("\r\n");

  /* Halt. UART4 dump above is the diagnostic — DS10 LEDs are on TIM3 PWM
   * (PA6/PA7/PB0/PB1, AF2) so they can't be GPIO-toggled here. PC4 has no
   * LED on DS10 (it was D6 on DS9). Keep the loop so the dump is re-sendable
   * via debugger inspection. */
  while (1) { __NOP(); }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32H7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DMA1 stream0 global interrupt.
  */
void DMA1_Stream0_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream0_IRQn 0 */

  /* USER CODE END DMA1_Stream0_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_sai1_a);
  /* USER CODE BEGIN DMA1_Stream0_IRQn 1 */

  /* USER CODE END DMA1_Stream0_IRQn 1 */
}

/**
  * @brief This function handles I2C1 event interrupt.
  */
void I2C1_EV_IRQHandler(void)
{
  /* USER CODE BEGIN I2C1_EV_IRQn 0 */

  /* USER CODE END I2C1_EV_IRQn 0 */
  HAL_I2C_EV_IRQHandler(&hi2c1);
  /* USER CODE BEGIN I2C1_EV_IRQn 1 */

  /* USER CODE END I2C1_EV_IRQn 1 */
}

/**
  * @brief This function handles I2C1 error interrupt.
  */
void I2C1_ER_IRQHandler(void)
{
  /* USER CODE BEGIN I2C1_ER_IRQn 0 */

  /* USER CODE END I2C1_ER_IRQn 0 */
  HAL_I2C_ER_IRQHandler(&hi2c1);
  /* USER CODE BEGIN I2C1_ER_IRQn 1 */

  /* USER CODE END I2C1_ER_IRQn 1 */
}

/**
  * @brief This function handles UART7 global interrupt.
  */
void UART7_IRQHandler(void)
{
  /* USER CODE BEGIN UART7_IRQn 0 */

  /* USER CODE END UART7_IRQn 0 */
  HAL_UART_IRQHandler(&huart7);
  /* USER CODE BEGIN UART7_IRQn 1 */

  /* USER CODE END UART7_IRQn 1 */
}

/**
  * @brief This function handles UART4 global interrupt (ESP32 communication).
  */
void UART4_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart4);
}

/**
  * @brief This function handles DMA1 stream1 global interrupt (SAI2_A).
  */
void DMA1_Stream1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_sai2_a);
}

/**
  * @brief This function handles DMA1 stream2 global interrupt (SAI2_B).
  */
void DMA1_Stream2_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_sai2_b);
}

/**
  * @brief This function handles SAI1 global interrupt.
  */
void SAI1_IRQHandler(void)
{
  HAL_SAI_IRQHandler(&hsai_BlockA1);
  HAL_SAI_IRQHandler(&hsai_BlockB1);
}

/**
  * @brief This function handles SAI2 global interrupt.
  */
void SAI2_IRQHandler(void)
{
  HAL_SAI_IRQHandler(&hsai_BlockA2);
  HAL_SAI_IRQHandler(&hsai_BlockB2);
}

/**
  * @brief This function handles DMA1 stream3 global interrupt (SAI1_B).
  */
void DMA1_Stream3_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_sai1_b);
}

/**
  * @brief This function handles DMA1 stream4 global interrupt (SAI3_B RX).
  */
void DMA1_Stream4_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_sai3_b);
}

/**
  * @brief This function handles SAI3 global interrupt (SAI3_B RX).
  */
void SAI3_IRQHandler(void)
{
  HAL_SAI_IRQHandler(&hsai_BlockB3);
}

/**
  * @brief This function handles DMAMUX1 overrun interrupt.
  */
void DMAMUX1_OVR_IRQHandler(void)
{
  /* USER CODE BEGIN DMAMUX1_OVR_IRQn 0 */

  /* USER CODE END DMAMUX1_OVR_IRQn 0 */
  /* USER CODE BEGIN DMAMUX1_OVR_IRQn 1 */

  /* USER CODE END DMAMUX1_OVR_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
