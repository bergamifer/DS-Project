/**
 * @file system_clock.c
 * @brief Clock configuration for STM32H743VIT6
 *
 * PLL1: System clock 480 MHz
 * PLL3: Audio clock 12.288 MHz (256 x 48 kHz)
 *
 * HSE = 25 MHz (WeAct board crystal)
 */

#include "main.h"

/**
 * @brief Configure system clocks
 *
 * HSE 25 MHz -> PLL1 -> SYSCLK 480 MHz
 *
 * PLL1 Configuration:
 *   - DIVM1 = 5  -> VCO input = 25/5 = 5 MHz
 *   - DIVN1 = 192 -> VCO output = 5 * 192 = 960 MHz
 *   - DIVP1 = 2  -> SYSCLK = 960/2 = 480 MHz
 *   - DIVQ1 = 4  -> PLL1Q = 240 MHz (for peripherals)
 *   - DIVR1 = 2  -> PLL1R = 480 MHz
 *
 * Bus clocks:
 *   - HCLK = SYSCLK/2 = 240 MHz (AXI/AHB)
 *   - APB1 = HCLK/2 = 120 MHz
 *   - APB2 = HCLK/2 = 120 MHz
 *   - APB3 = HCLK/2 = 120 MHz
 *   - APB4 = HCLK/2 = 120 MHz
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // Supply configuration - LDO
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    // Configure voltage scaling for 480 MHz
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    // Configure HSE and PLL1
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 5;      // VCO input = 25/5 = 5 MHz
    RCC_OscInitStruct.PLL.PLLN = 192;    // VCO output = 5 * 192 = 960 MHz
    RCC_OscInitStruct.PLL.PLLP = 2;      // SYSCLK = 960/2 = 480 MHz
    RCC_OscInitStruct.PLL.PLLQ = 4;      // PLL1Q = 240 MHz
    RCC_OscInitStruct.PLL.PLLR = 2;      // PLL1R = 480 MHz
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;  // 4-8 MHz range
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;  // Wide VCO 192-960 MHz
    RCC_OscInitStruct.PLL.PLLFRACN = 0;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    // Configure system clocks
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;    // HCLK = 240 MHz
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;   // D1PCLK1 = 120 MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;   // PCLK1 = 120 MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;   // PCLK2 = 120 MHz
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;   // D3PCLK1 = 120 MHz

    // Flash latency for 240 MHz HCLK with VOS0
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief Configure PLL3 for audio (12.288 MHz MCLK)
 *
 * Target: 12.288 MHz = 256 × 48 kHz
 *
 * PLL3 Configuration:
 *   HSE 25 MHz -> DIVM3=5 -> 5 MHz VCO input
 *   5 MHz × DIVN3=68 = 340 MHz VCO output (fractional for precision)
 *   340 MHz / DIVP3=28 ≈ 12.142857 MHz
 *
 * For exact 12.288 MHz, use fractional:
 *   VCO = 5 MHz × (68 + 1228/8192) = 344.064 MHz
 *   MCLK = 344.064 / 28 = 12.288 MHz ✓
 *
 * FRACN = (0.1228 × 8192) ≈ 1006
 */
void Audio_PLL3_Config(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    // Configure PLL3 for SAI clock
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI1 | RCC_PERIPHCLK_SAI4B;
    PeriphClkInitStruct.PLL3.PLL3M = 5;       // VCO input = 25/5 = 5 MHz
    PeriphClkInitStruct.PLL3.PLL3N = 68;      // Integer part
    PeriphClkInitStruct.PLL3.PLL3P = 28;      // MCLK divider
    PeriphClkInitStruct.PLL3.PLL3Q = 28;      // Not used
    PeriphClkInitStruct.PLL3.PLL3R = 28;      // Not used
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;  // 4-8 MHz
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOMEDIUM;
    PeriphClkInitStruct.PLL3.PLL3FRACN = 1006; // Fractional for exact 12.288 MHz

    // SAI1 and SAI4 use PLL3P
    PeriphClkInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL3;
    PeriphClkInitStruct.Sai4BClockSelection = RCC_SAI4BCLKSOURCE_PLL3;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    // Enable PLL3
    __HAL_RCC_PLL3_ENABLE();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_PLL3RDY) == 0) {}

    // Enable PLL3 fractional divider
    __HAL_RCC_PLL3FRACN_ENABLE();
}

/**
 * @brief Get current system clock frequencies (for debug)
 */
void Debug_PrintClock(void)
{
    char buf[128];
    uint32_t sysclk = HAL_RCC_GetSysClockFreq();
    uint32_t hclk = HAL_RCC_GetHCLKFreq();
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();

    Debug_Print("\r\n=== Clock Configuration ===\r\n");

    sprintf(buf, "SYSCLK: %lu MHz\r\n", sysclk / 1000000);
    Debug_Print(buf);

    sprintf(buf, "HCLK:   %lu MHz\r\n", hclk / 1000000);
    Debug_Print(buf);

    sprintf(buf, "PCLK1:  %lu MHz\r\n", pclk1 / 1000000);
    Debug_Print(buf);

    sprintf(buf, "PCLK2:  %lu MHz\r\n", pclk2 / 1000000);
    Debug_Print(buf);

    Debug_Print("PLL3:   12.288 MHz (audio)\r\n");
    Debug_Print("===========================\r\n");
}
