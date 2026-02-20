/**
 * @file main_led_test.c
 * @brief DS9 Board Sanity Test — LED chase sequence
 *
 * Tests:
 *  1. USB DFU flash works (MCU alive)
 *  2. LEDs D6/D7/D11/D10 all light up in sequence
 *
 * No SAI, no DMA, no I2C, no audio.
 * Clock: HSI 64MHz (no external crystal needed).
 *
 * Build:  make led-test
 * Flash:  make led-flash   (DFU via USB-C)
 *
 * DFU mode: Hold BOOT0 button, press/release RESET, release BOOT0.
 *           Confirm with: dfu-util -l
 */

#include "stm32h7xx_hal.h"

/* LEDs — Active HIGH, series resistor to GND */
#define LED_D6_PIN      GPIO_PIN_4   /* PC4 */
#define LED_D7_PIN      GPIO_PIN_5   /* PC5 */
#define LED_D11_PIN     GPIO_PIN_0   /* PB0 */
#define LED_D10_PIN     GPIO_PIN_1   /* PB1 */
#define LED_GPIOB       GPIOB
#define LED_GPIOC       GPIOC

#define CHASE_DELAY_MS  300

static void SystemClock_Config(void);
static void LED_Init(void);
static void LED_Set(GPIO_TypeDef *port, uint16_t pin, uint8_t on);
static void LED_AllOff(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    LED_Init();

    /* Startup: all on for 500ms to confirm all 4 LEDs work */
    HAL_GPIO_WritePin(LED_GPIOC, LED_D6_PIN | LED_D7_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_GPIOB, LED_D11_PIN | LED_D10_PIN, GPIO_PIN_SET);
    HAL_Delay(500);
    LED_AllOff();
    HAL_Delay(200);

    /* Chase: D6 → D7 → D11 → D10 → repeat */
    while (1)
    {
        LED_Set(LED_GPIOC, LED_D6_PIN,  1); HAL_Delay(CHASE_DELAY_MS); LED_Set(LED_GPIOC, LED_D6_PIN,  0);
        LED_Set(LED_GPIOC, LED_D7_PIN,  1); HAL_Delay(CHASE_DELAY_MS); LED_Set(LED_GPIOC, LED_D7_PIN,  0);
        LED_Set(LED_GPIOB, LED_D11_PIN, 1); HAL_Delay(CHASE_DELAY_MS); LED_Set(LED_GPIOB, LED_D11_PIN, 0);
        LED_Set(LED_GPIOB, LED_D10_PIN, 1); HAL_Delay(CHASE_DELAY_MS); LED_Set(LED_GPIOB, LED_D10_PIN, 0);
    }
}

/* HSI 64MHz — no external crystal required */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Supply config: LDO (matches USE_PWR_LDO_SUPPLY define) */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY));

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState       = RCC_HSI_DIV1;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState   = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                         RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clk.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider  = RCC_HCLK_DIV1;
    clk.APB3CLKDivider = RCC_APB3_DIV1;
    clk.APB1CLKDivider = RCC_APB1_DIV1;
    clk.APB2CLKDivider = RCC_APB2_DIV1;
    clk.APB4CLKDivider = RCC_APB4_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1);
}

static void LED_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Start LOW (LEDs off) */
    HAL_GPIO_WritePin(LED_GPIOC, LED_D6_PIN | LED_D7_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GPIOB, LED_D11_PIN | LED_D10_PIN, GPIO_PIN_RESET);

    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = LED_D6_PIN | LED_D7_PIN;
    HAL_GPIO_Init(LED_GPIOC, &gpio);

    gpio.Pin = LED_D11_PIN | LED_D10_PIN;
    HAL_GPIO_Init(LED_GPIOB, &gpio);
}

static void LED_Set(GPIO_TypeDef *port, uint16_t pin, uint8_t on)
{
    HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void LED_AllOff(void)
{
    HAL_GPIO_WritePin(LED_GPIOC, LED_D6_PIN | LED_D7_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GPIOB, LED_D11_PIN | LED_D10_PIN, GPIO_PIN_RESET);
}

/* HAL requires this callback */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
