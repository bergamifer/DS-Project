/**
 * @file uart_debug.c
 * @brief UART7 debug interface for ESP32 communication
 */

#include "main.h"
#include <string.h>
#include <stdio.h>

UART_HandleTypeDef huart7;

static volatile uint8_t rx_buffer[256];
static volatile uint16_t rx_index = 0;

/**
 * @brief Initialize UART7 for debug/ESP32 communication
 *
 * PA8 = RX, PA15 = TX
 * 115200 baud, 8N1
 */
void UART7_Init(void)
{
    // Enable UART7 clock
    __HAL_RCC_UART7_CLK_ENABLE();

    // Initialize GPIO
    GPIO_UART7_Init();

    // Configure UART7
    huart7.Instance = UART7;
    huart7.Init.BaudRate = UART_BAUDRATE;
    huart7.Init.WordLength = UART_WORDLENGTH_8B;
    huart7.Init.StopBits = UART_STOPBITS_1;
    huart7.Init.Parity = UART_PARITY_NONE;
    huart7.Init.Mode = UART_MODE_TX_RX;
    huart7.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart7.Init.OverSampling = UART_OVERSAMPLING_16;
    huart7.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart7.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart7.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart7) != HAL_OK)
    {
        Error_Handler();
    }

    // Enable FIFO mode
    if (HAL_UARTEx_SetTxFifoThreshold(&huart7, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart7, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_EnableFifoMode(&huart7) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief Initialize debug system
 */
void Debug_Init(void)
{
    UART7_Init();
    Debug_Print("\r\n");
    Debug_Print("====================================\r\n");
    Debug_Print("  Domosonica DS - Audio Processor\r\n");
    Debug_Print("  STM32H743VIT6 @ 480 MHz\r\n");
    Debug_Print("====================================\r\n");
}

/**
 * @brief Print string to debug UART
 * @param msg Null-terminated string
 */
void Debug_Print(const char *msg)
{
    HAL_UART_Transmit(&huart7, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

/**
 * @brief Print formatted string to debug UART
 * @param format Printf format string
 */
void Debug_Printf(const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Debug_Print(buffer);
}

/**
 * @brief Print hex dump
 * @param data Pointer to data
 * @param len Length of data
 */
void Debug_HexDump(const uint8_t *data, uint16_t len)
{
    char buf[8];
    for (uint16_t i = 0; i < len; i++)
    {
        sprintf(buf, "%02X ", data[i]);
        Debug_Print(buf);
        if ((i + 1) % 16 == 0)
        {
            Debug_Print("\r\n");
        }
    }
    if (len % 16 != 0)
    {
        Debug_Print("\r\n");
    }
}

/**
 * @brief Print system status
 */
void Debug_PrintStatus(void)
{
    Debug_Print("\r\n--- System Status ---\r\n");

    // DAC status
    uint8_t dac0_mute = HAL_GPIO_ReadPin(DAC0_AMUTEO_PORT, DAC0_AMUTEO_PIN);
    uint8_t dac1_mute = HAL_GPIO_ReadPin(DAC1_AMUTEO_PORT, DAC1_AMUTEO_PIN);
    Debug_Printf("DAC0 Mute: %s\r\n", dac0_mute ? "MUTED" : "OK");
    Debug_Printf("DAC1 Mute: %s\r\n", dac1_mute ? "MUTED" : "OK");

    // Clock status
    Debug_PrintClock();
}

/**
 * @brief Retarget printf to UART (optional)
 */
#ifdef __GNUC__
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart7, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}
#endif
