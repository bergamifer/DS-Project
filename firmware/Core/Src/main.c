/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include "uart_protocol.h"
#include "pcm1690.h"
#include "pin_definitions.h"
#include "eeprom_config.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AUDIO_SAMPLE_RATE    48000
#define AUDIO_BUFFER_SAMPLES 480      // 10ms @ 48kHz
#define AUDIO_CHANNELS       8        // TDM8 (8 channels per PCM1690)
#define AUDIO_BUFFER_SIZE    (AUDIO_BUFFER_SAMPLES * AUDIO_CHANNELS)

#define CPU_FREQ_MHZ         480      // CPU clock after PLL1 config

#define ADC_CHANNELS         2        // PCM1802 stereo input
#define ADC_BUFFER_SAMPLES   AUDIO_BUFFER_SAMPLES  // 480 samples = 10ms
#define ADC_BUFFER_SIZE      (ADC_BUFFER_SAMPLES * ADC_CHANNELS)   // 960 samples

// Beamforming delay line
#define DELAY_LINE_SIZE      1024     // power of 2 for fast & mask
#define DELAY_LINE_MASK      (DELAY_LINE_SIZE - 1)
#define MAX_DELAY_SAMPLES    64       // ~1.33ms @ 48kHz = ~45.7cm at 343 m/s
#define BEAM_NUM_CHANNELS    32
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Beamforming config — 32 channels (expandable from ESP32's 16-ch ds_config_t) */
typedef struct {
    uint8_t  master_gain;                   // 0-255 → 0.0-1.0
    uint8_t  mute_global;                   // 0=unmuted, 1=muted
    uint16_t delays[BEAM_NUM_CHANNELS];     // delay in samples per channel
    uint8_t  gains[BEAM_NUM_CHANNELS];      // gain per channel 0-255
    uint32_t mutes;                         // bitfield: bit N = channel N muted
} beam_config_t;

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c1;

SAI_HandleTypeDef hsai_BlockA1;
SAI_HandleTypeDef hsai_BlockB1;
SAI_HandleTypeDef hsai_BlockA2;
SAI_HandleTypeDef hsai_BlockB2;
SAI_HandleTypeDef hsai_BlockB3;
DMA_HandleTypeDef hdma_sai1_a;
DMA_HandleTypeDef hdma_sai1_b;
DMA_HandleTypeDef hdma_sai2_a;
DMA_HandleTypeDef hdma_sai2_b;
DMA_HandleTypeDef hdma_sai3_b;

UART_HandleTypeDef huart7;
UART_HandleTypeDef huart4;

ADC_HandleTypeDef hadc3;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */

/* ===== DMA Double Buffer (ping-pong) =====
 * Layout: [HALF_A (480 × 8ch)][HALF_B (480 × 8ch)]
 *          ^                    ^
 *          HalfCplt fills A     FullCplt fills B
 *
 * TDM8: 8 channels interleaved per sample frame
 * While DMA sends HALF_B → CPU fills HALF_A (and vice versa)
 * MUST be in RAM_D2 for DMA1 access (DTCMRAM NOT DMA-accessible on STM32H7!)
 */
static int32_t audio_buffer[AUDIO_BUFFER_SIZE * 2] __attribute__((section(".dma_buffer"), aligned(32)));
static int32_t audio_buffer_1b[AUDIO_BUFFER_SIZE * 2] __attribute__((section(".dma_buffer"), aligned(32)));
static int32_t audio_buffer_2a[AUDIO_BUFFER_SIZE * 2] __attribute__((section(".dma_buffer"), aligned(32)));
static int32_t audio_buffer_2b[AUDIO_BUFFER_SIZE * 2] __attribute__((section(".dma_buffer"), aligned(32)));
static int32_t audio_buffer_3b_rx[ADC_BUFFER_SIZE * 2] __attribute__((section(".dma_buffer"), aligned(32)));

// Circular delay lines for beamforming (one per beam/ADC channel)
static int32_t delay_line_L[DELAY_LINE_SIZE] __attribute__((section(".dma_buffer"), aligned(32)));
static int32_t delay_line_R[DELAY_LINE_SIZE] __attribute__((section(".dma_buffer"), aligned(32)));
static uint32_t delay_write_pos = 0;  // Current write index into delay lines

// Active beamforming config (copied from ESP32 ds_config_t on update)
static beam_config_t beam_config;

// PCM1690 DAC pin configs (DS9 v2.0 — from pin_definitions.h)
static const PCM1690_Pins dac_pins[4] = {
    { DAC0_RST_PORT, DAC0_RST_PIN, DAC0_AMUTEI_PORT, DAC0_AMUTEI_PIN }, // DAC0 U3
    { DAC1_RST_PORT, DAC1_RST_PIN, DAC1_AMUTEI_PORT, DAC1_AMUTEI_PIN }, // DAC1 U4
    { DAC2_RST_PORT, DAC2_RST_PIN, DAC2_AMUTEI_PORT, DAC2_AMUTEI_PIN }, // DAC2 U2
    { DAC3_RST_PORT, DAC3_RST_PIN, DAC3_AMUTEI_PORT, DAC3_AMUTEI_PIN }, // DAC3 U8
};
static const uint8_t dac_addrs[4] = {
    PCM1690_ADDR_DAC0, PCM1690_ADDR_DAC1, PCM1690_ADDR_DAC2, PCM1690_ADDR_DAC3
};

// EEPROM deferred save flag (set when config changes, save in low-priority window)
static volatile uint8_t eeprom_save_pending = 0;

// LED dimmer state machine (TIM3: CH3=D11/PB0, CH4=D10/PB1)
// Phase 0: D11 fades 0→max in 3s, D10 off
// Phase 1: D10 fades 0→max in 3s, D11 off
#define DIMMER_STEPS     300UL
#define DIMMER_STEP_CYC  (480000000UL / 100UL)  // 10ms @ 480MHz
#define DIMMER_ARR       999UL
static uint8_t  dimmer_phase    = 0;
static uint32_t dimmer_step     = 0;
static uint32_t last_dimmer_dwt = 0;

// ===== ISR → Main loop deferred processing flags =====
// ISR sets flag, main loop processes buffer and clears flag
// This keeps ISR execution < 20 cycles (vs ~50000 with sinf())
static volatile uint8_t need_fill_first_half = 0;
static volatile uint8_t need_fill_second_half = 0;
static volatile uint8_t need_fill_2a_first_half = 0;
static volatile uint8_t need_fill_2a_second_half = 0;
static volatile uint8_t need_fill_1b_first_half = 0;
static volatile uint8_t need_fill_1b_second_half = 0;
static volatile uint8_t need_fill_2b_first_half = 0;
static volatile uint8_t need_fill_2b_second_half = 0;
static volatile uint8_t rx_3b_first_half_ready = 0;
static volatile uint8_t rx_3b_second_half_ready = 0;

// Underrun detection: flag set but not serviced before next ISR
static volatile uint32_t underrun_count = 0;

// Callback counters for debug
static volatile uint32_t half_complete_count = 0;
static volatile uint32_t full_complete_count = 0;
static volatile uint32_t error_count = 0;
static volatile uint32_t last_sai_error = 0;

// ===== Latency & Jitter measurement via DWT cycle counter =====
// DWT->CYCCNT runs at CPU clock (480MHz → 2.08ns/cycle)
static volatile uint32_t last_half_isr_cycles = 0;
static volatile uint32_t last_full_isr_cycles = 0;

// Jitter tracking: callback period variation (Half + Full combined)
static volatile uint32_t jitter_min_cycles = 0xFFFFFFFF;
static volatile uint32_t jitter_max_cycles = 0;
static volatile uint64_t jitter_sum_cycles = 0;
static volatile uint32_t jitter_sample_count = 0;

// Processing latency: fill buffer + cache clean
static volatile uint32_t fill_min_cycles = 0xFFFFFFFF;
static volatile uint32_t fill_max_cycles = 0;
static volatile uint64_t fill_sum_cycles = 0;
static volatile uint32_t fill_sample_count = 0;

// UART TX buffer
static char uart_buf[128];

// Test cycle counter (for 60s status reports)
static uint32_t test_cycle = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_UART7_Init(void);
static void MX_SAI1_Init(void);
static void MX_UART4_Init(void);
static void MX_SAI2_Init(void);
static void MX_SAI3_Init(void);
static void MX_ADC3_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
static int32_t Read_CPU_Temperature(void);
static void Fill_Audio_Beamform(int32_t *buffer, uint32_t samples, uint32_t ch_offset);
static void Process_ADC_Half(int32_t *rx_buf, uint32_t samples);
static void UART_Print(const char *str);
static void Print_DMA_Config(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_UART4_Init();   /* UART4 (PA0/PA1) first — debug output on DS9 */
  MX_UART7_Init();
  MX_SAI1_Init();
  MX_SAI2_Init();
  MX_SAI3_Init();
  /* USER CODE BEGIN 2 */

  // === LED diagnostics init: D6=PC4 (boot), D7=PC5 (TX), D11=PB0 (RX) ===
  // GPIOC and GPIOB clocks already enabled in MX_GPIO_Init.
  {
    GPIO_InitTypeDef gpio_led = {0};
    gpio_led.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_led.Pull  = GPIO_NOPULL;
    gpio_led.Speed = GPIO_SPEED_FREQ_LOW;
    // D6 (PC4) + D7 (PC5) on GPIOC
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);
    gpio_led.Pin = LED1_PIN | LED2_PIN;
    HAL_GPIO_Init(LED1_PORT, &gpio_led);
    // D11 (PB0) and D10 (PB1) owned by TIM3 PWM — not configured as GPIO output
    // Brief D6 flash: confirms we reached this point in init
    // Use software busy-wait — HAL_Delay() depends on SysTick which can be starved
    // by priority-0 SAI/I2C ISRs enabled in MspInit. ~200ms @ 480MHz.
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
    for (volatile uint32_t i = 0; i < 24000000UL; i++) { __NOP(); }
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
  }

  // PE3 is SAI1_SD_B — shared with DAC1_RST, resolved in init sequence

  // Startup message
  UART_Print("\r\n=== DS9 Beamforming Audio ===\r\n");
  UART_Print("CPU: 480MHz (VOS0), HCLK: 240MHz, APB: 120MHz\r\n");
  UART_Print("SAI1_A+B: TDM8 ch0-15, SAI2_A+B: TDM8 ch16-31, SAI3_B: RX 2ch\r\n");
  UART_Print("Mode: Beamforming (ADC L -> delay lines -> 32ch DAC)\r\n\r\n");

  // === ADC3 temperature sensor ===
  MX_ADC3_Init();

  // === TIM3 PWM — LED dimmer (D11=PB0/CH3, D10=PB1/CH4) ===
  MX_TIM3_Init();

  // === Beamforming init ===
  // Zero delay lines
  memset(delay_line_L, 0, sizeof(delay_line_L));
  memset(delay_line_R, 0, sizeof(delay_line_R));
  delay_write_pos = 0;

  // Default beam config: all gains max, no delays, unmuted
  beam_config.master_gain = 255;
  beam_config.mute_global = 0;
  memset(beam_config.delays, 0, sizeof(beam_config.delays));
  memset(beam_config.gains, 255, sizeof(beam_config.gains));  // 0xFF = unity gain
  beam_config.mutes = 0;
  UART_Print("Beamforming initialized (delay lines zeroed, gains=max)\r\n");

  // === EEPROM init + load saved config ===
  {
    UART_Print("EEPROM: Initializing I2C4 AF6 (PB6/PB7)...\r\n");
    HAL_StatusTypeDef i2c_eep_st = EEPROM_Init();
    sprintf(uart_buf, "EEPROM: I2C4 init=%d, available=%u\r\n", (int)i2c_eep_st, EEPROM_IsAvailable());
    UART_Print(uart_buf);

    if (EEPROM_IsAvailable()) {
      eeprom_status_t eep_status = EEPROM_LoadConfig(&beam_config, sizeof(beam_config));
      if (eep_status == EEPROM_OK) {
        UART_Print("EEPROM: Config loaded OK\r\n");
        sprintf(uart_buf, "  master_gain=%u mute=%u mutes=0x%08lX\r\n",
                beam_config.master_gain, beam_config.mute_global, beam_config.mutes);
        UART_Print(uart_buf);
      } else if (eep_status == EEPROM_ERR_MAGIC) {
        UART_Print("EEPROM: No saved config (first boot) — using defaults\r\n");
      } else if (eep_status == EEPROM_ERR_CRC) {
        UART_Print("EEPROM: CRC error — using defaults\r\n");
      } else {
        sprintf(uart_buf, "EEPROM: Load error %d — using defaults\r\n", (int)eep_status);
        UART_Print(uart_buf);
      }
    } else {
      UART_Print("EEPROM: Not detected on I2C3 — using defaults\r\n");
    }
  }

  // Pre-fill TX buffers with silence (zeros)
  UART_Print("DBG: memset audio_buffer...\r\n");
  memset(audio_buffer, 0, sizeof(audio_buffer));
  __DSB();  /* D-cache not enabled — DSB ensures DMA sees writes */
  UART_Print("DBG: memset audio_buffer_1b...\r\n");
  memset(audio_buffer_1b, 0, sizeof(audio_buffer_1b));
  __DSB();
  UART_Print("TX buffers pre-filled (silence)\r\n");

  // Start SAI1_B (slave) BEFORE SAI1_A (master) - slave must be ready when master clocks
  HAL_StatusTypeDef status;
  status = HAL_SAI_Transmit_DMA(&hsai_BlockB1, (uint8_t*)audio_buffer_1b, AUDIO_BUFFER_SIZE * 2);
  if (status == HAL_OK) {
    UART_Print("SAI1_B DMA Started OK (slave, waiting for master)\r\n");
  } else {
    sprintf(uart_buf, "SAI1_B DMA FAILED! Status: %d, Error: 0x%08lX\r\n",
            status, HAL_SAI_GetError(&hsai_BlockB1));
    UART_Print(uart_buf);
  }

  status = HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*)audio_buffer, AUDIO_BUFFER_SIZE * 2);
  if (status == HAL_OK) {
    UART_Print("SAI1_A DMA Started OK (master)\r\n");
    Print_DMA_Config();
  } else {
    sprintf(uart_buf, "SAI1_A DMA FAILED! Status: %d, Error: 0x%08lX\r\n",
            status, HAL_SAI_GetError(&hsai_BlockA1));
    UART_Print(uart_buf);
  }

  // === PCM1690 DAC initialization via I2C ===
  // MCLK is now running on PE2 (SAI1_A master started above).
  // PCM1690 needs SCKI after RST release to complete POR (3846 cycles).
  {
    // Configure RST/AMUTEI GPIOs as output push-pull (drive LOW initially)
    GPIO_InitTypeDef gpio_dac = {0};
    gpio_dac.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_dac.Pull  = GPIO_NOPULL;
    gpio_dac.Speed = GPIO_SPEED_FREQ_LOW;

    // GPIOE: PE10(RST0), PE3(RST1), PE11(AMUTEI0), PE1(AMUTEI1)
    HAL_GPIO_WritePin(GPIOE, DAC0_RST_PIN | DAC1_RST_PIN | DAC0_AMUTEI_PIN | DAC1_AMUTEI_PIN, GPIO_PIN_RESET);
    gpio_dac.Pin = DAC0_RST_PIN | DAC1_RST_PIN | DAC0_AMUTEI_PIN | DAC1_AMUTEI_PIN;
    HAL_GPIO_Init(GPIOE, &gpio_dac);

    // GPIOC: PC10(RST2), PC11(AMUTEI2)
    HAL_GPIO_WritePin(GPIOC, DAC2_RST_PIN | DAC2_AMUTEI_PIN, GPIO_PIN_RESET);
    gpio_dac.Pin = DAC2_RST_PIN | DAC2_AMUTEI_PIN;
    HAL_GPIO_Init(GPIOC, &gpio_dac);

    // GPIOB: PB15(RST3), PB14(AMUTEI3)
    HAL_GPIO_WritePin(GPIOB, DAC3_RST_PIN | DAC3_AMUTEI_PIN, GPIO_PIN_RESET);
    gpio_dac.Pin = DAC3_RST_PIN | DAC3_AMUTEI_PIN;
    HAL_GPIO_Init(GPIOB, &gpio_dac);

    UART_Print("\r\nInitializing PCM1690 DACs via I2C (MCLK running)...\r\n");
    const char *dac_names[4] = { "DAC0 U3", "DAC1 U4", "DAC2 U2", "DAC3 U8" };
    uint8_t dacs_ok = 0;
    for (int i = 0; i < 4; i++) {
      PCM1690_Status ds = PCM1690_Init(&hi2c1, dac_addrs[i], &dac_pins[i]);
      sprintf(uart_buf, "  %s (0x%02X): %s\r\n", dac_names[i], dac_addrs[i],
              ds == PCM1690_OK ? "OK" : "FAIL");
      UART_Print(uart_buf);
      if (ds == PCM1690_OK) dacs_ok++;
    }
    sprintf(uart_buf, "PCM1690: %d/4 initialized\r\n", dacs_ok);
    UART_Print(uart_buf);

    // Handle pin conflicts: reconfigure shared pins for SAI after DAC init
    // PE3 (DAC1_RST) → SAI1_SD_B (AF6) — RST already released, no longer needed
    gpio_dac.Pin       = GPIO_PIN_3;
    gpio_dac.Mode      = GPIO_MODE_AF_PP;
    gpio_dac.Pull      = GPIO_NOPULL;
    gpio_dac.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio_dac.Alternate = GPIO_AF6_SAI1;
    HAL_GPIO_Init(GPIOE, &gpio_dac);

    // PE11 (DAC0_AMUTEI) → SAI2_SD_B (AF10) — DAC0 unmuted via soft-mute register
    gpio_dac.Pin       = GPIO_PIN_11;
    gpio_dac.Alternate = GPIO_AF10_SAI2;
    HAL_GPIO_Init(GPIOE, &gpio_dac);

    UART_Print("PE3→SAI1_SD_B, PE11→SAI2_SD_B (pin conflicts resolved)\r\n");
  }

  // === SAI2 (DAC2 + DAC3) DMA start ===
  // Pre-fill SAI2 buffers with silence
  memset(audio_buffer_2a, 0, sizeof(audio_buffer_2a));
  __DSB();
  memset(audio_buffer_2b, 0, sizeof(audio_buffer_2b));
  __DSB();

  // Start SAI2_B (slave) BEFORE SAI2_A (master) - slave must be ready when master clocks
  status = HAL_SAI_Transmit_DMA(&hsai_BlockB2, (uint8_t*)audio_buffer_2b, AUDIO_BUFFER_SIZE * 2);
  if (status == HAL_OK) {
    UART_Print("SAI2_B DMA Started OK (slave, waiting for master)\r\n");
  } else {
    sprintf(uart_buf, "SAI2_B DMA FAILED! Status: %d, Error: 0x%08lX\r\n",
            status, HAL_SAI_GetError(&hsai_BlockB2));
    UART_Print(uart_buf);
  }

  status = HAL_SAI_Transmit_DMA(&hsai_BlockA2, (uint8_t*)audio_buffer_2a, AUDIO_BUFFER_SIZE * 2);
  if (status == HAL_OK) {
    UART_Print("SAI2_A DMA Started OK (master)\r\n");
  } else {
    sprintf(uart_buf, "SAI2_A DMA FAILED! Status: %d, Error: 0x%08lX\r\n",
            status, HAL_SAI_GetError(&hsai_BlockA2));
    UART_Print(uart_buf);
  }

  // === SAI3_B RX DMA start (ADC input) ===
  __DSB();
  status = HAL_SAI_Receive_DMA(&hsai_BlockB3, (uint8_t*)audio_buffer_3b_rx, ADC_BUFFER_SIZE * 2);
  if (status == HAL_OK) {
    UART_Print("SAI3_B RX DMA Started OK (ADC master)\r\n");
  } else {
    sprintf(uart_buf, "SAI3_B RX DMA FAILED! Status: %d, Error: 0x%08lX\r\n",
            status, HAL_SAI_GetError(&hsai_BlockB3));
    UART_Print(uart_buf);
  }

  // === ESP32 UART protocol init ===
  UART_Protocol_Init(&huart4, &huart7);
  HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);  // D6 ON: full init complete

  // Reset counters + flags (underruns during init are expected)
  need_fill_first_half = 0;
  need_fill_second_half = 0;
  need_fill_1b_first_half = 0;
  need_fill_1b_second_half = 0;
  need_fill_2a_first_half = 0;
  need_fill_2a_second_half = 0;
  need_fill_2b_first_half = 0;
  need_fill_2b_second_half = 0;
  rx_3b_first_half_ready = 0;
  rx_3b_second_half_ready = 0;
  half_complete_count = 0;
  full_complete_count = 0;
  underrun_count = 0;
  error_count = 0;
  __DSB();

  // Enable DWT cycle counter for latency measurement (ARM Cortex-M7)
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  // Reset jitter tracking AFTER DWT enable
  __disable_irq();
  last_half_isr_cycles = 0;
  last_full_isr_cycles = 0;
  jitter_min_cycles = 0xFFFFFFFF;
  jitter_max_cycles = 0;
  jitter_sum_cycles = 0;
  jitter_sample_count = 0;
  fill_min_cycles = 0xFFFFFFFF;
  fill_max_cycles = 0;
  fill_sum_cycles = 0;
  fill_sample_count = 0;
  __enable_irq();
  UART_Print("DWT cycle counter enabled (latency measurement)\r\n");

  // Variables for main loop
  uint32_t last_half = 0;
  uint32_t last_full = 0;
  uint32_t test_start = HAL_GetTick();
  // DWT-based 1s heartbeat — works even if SysTick is frozen
  uint32_t last_hb_dwt = DWT->CYCCNT;

  UART_Print("[STM32] main loop started — SAI+DMA running\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // ===== Deferred buffer fill: beamform from delay line =====
    // ISR sets flag, we process here in main context
    // SAI1_A: channels 0-7
    if (need_fill_first_half) {
      need_fill_first_half = 0;
      __DSB();
      uint32_t t0 = DWT->CYCCNT;
      Fill_Audio_Beamform(&audio_buffer[0], AUDIO_BUFFER_SAMPLES, 0);
      __DSB();
      uint32_t dt = DWT->CYCCNT - t0;
      if (dt < fill_min_cycles) fill_min_cycles = dt;
      if (dt > fill_max_cycles) fill_max_cycles = dt;
      fill_sum_cycles += dt;
      fill_sample_count++;
    }
    if (need_fill_second_half) {
      need_fill_second_half = 0;
      __DSB();
      uint32_t t0 = DWT->CYCCNT;
      Fill_Audio_Beamform(&audio_buffer[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SAMPLES, 0);
      __DSB();
      uint32_t dt = DWT->CYCCNT - t0;
      if (dt < fill_min_cycles) fill_min_cycles = dt;
      if (dt > fill_max_cycles) fill_max_cycles = dt;
      fill_sum_cycles += dt;
      fill_sample_count++;
    }

    // SAI1_B: channels 8-15
    if (need_fill_1b_first_half) {
      need_fill_1b_first_half = 0;
      __DSB();
      Fill_Audio_Beamform(&audio_buffer_1b[0], AUDIO_BUFFER_SAMPLES, 8);
      __DSB();
    }
    if (need_fill_1b_second_half) {
      need_fill_1b_second_half = 0;
      __DSB();
      Fill_Audio_Beamform(&audio_buffer_1b[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SAMPLES, 8);
      __DSB();
    }

    // SAI2_A: channels 16-23
    if (need_fill_2a_first_half) {
      need_fill_2a_first_half = 0;
      __DSB();
      Fill_Audio_Beamform(&audio_buffer_2a[0], AUDIO_BUFFER_SAMPLES, 16);
      __DSB();
    }
    if (need_fill_2a_second_half) {
      need_fill_2a_second_half = 0;
      __DSB();
      Fill_Audio_Beamform(&audio_buffer_2a[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SAMPLES, 16);
      __DSB();
    }

    // SAI2_B: channels 24-31
    if (need_fill_2b_first_half) {
      need_fill_2b_first_half = 0;
      __DSB();
      Fill_Audio_Beamform(&audio_buffer_2b[0], AUDIO_BUFFER_SAMPLES, 24);
      __DSB();
    }
    if (need_fill_2b_second_half) {
      need_fill_2b_second_half = 0;
      __DSB();
      Fill_Audio_Beamform(&audio_buffer_2b[AUDIO_BUFFER_SIZE], AUDIO_BUFFER_SAMPLES, 24);
      __DSB();
    }

    // ===== SAI3_B RX → feed delay lines (MUST run before TX fills) =====
    if (rx_3b_first_half_ready) {
      rx_3b_first_half_ready = 0;
      __DSB();
      __DSB();
      Process_ADC_Half(&audio_buffer_3b_rx[0], ADC_BUFFER_SAMPLES);
    }
    if (rx_3b_second_half_ready) {
      rx_3b_second_half_ready = 0;
      __DSB();
      __DSB();
      Process_ADC_Half(&audio_buffer_3b_rx[ADC_BUFFER_SIZE], ADC_BUFFER_SAMPLES);
    }

    // ===== Check for new ESP32 config → apply to beam_config =====
    if (UART_Protocol_HasNewConfig()) {
      const ds_config_t *cfg = UART_Protocol_GetConfig();
      beam_config.master_gain = cfg->master_gain;
      beam_config.mute_global = cfg->mute_global;
      // Map 16 ESP32 channels into first 16 beam channels
      for (uint32_t i = 0; i < CFG_NUM_CHANNELS; i++) {
        beam_config.delays[i] = cfg->delays[i];
        beam_config.gains[i] = cfg->gains[i];
      }
      beam_config.mutes = (uint32_t)cfg->mutes;  // lower 16 bits
      UART_Protocol_PrintConfig();
      UART_Print("Config applied to beamformer\r\n");

      // Defer EEPROM save — will execute when no audio fills are pending
      if (EEPROM_IsAvailable()) {
        eeprom_save_pending = 1;
      }
    }

    // ===== LED dimmer — TIM3 PWM step update (10ms per step, 300 steps = 3s) =====
    if (last_dimmer_dwt != 0 && (DWT->CYCCNT - last_dimmer_dwt) >= DIMMER_STEP_CYC) {
      last_dimmer_dwt += DIMMER_STEP_CYC;
      uint32_t ccr = (dimmer_step * DIMMER_ARR) / (DIMMER_STEPS - 1);
      if (dimmer_phase == 0) {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, ccr);  // D11 rising
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0);    // D10 off
      } else {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);    // D11 off
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, ccr);  // D10 rising
      }
      dimmer_step++;
      if (dimmer_step >= DIMMER_STEPS) {
        dimmer_step = 0;
        dimmer_phase ^= 1;
      }
    }

    // ===== Deferred EEPROM save (blocks ~12ms, causes underruns — acceptable) =====
    if (eeprom_save_pending) {
      eeprom_save_pending = 0;
      UART_Print("EEPROM: Saving config...\r\n");
      eeprom_status_t eep_st = EEPROM_SaveConfig(&beam_config, sizeof(beam_config));
      if (eep_st == EEPROM_OK) {
        UART_Print("EEPROM: Config saved OK\r\n");
      } else {
        sprintf(uart_buf, "EEPROM: Save FAILED (err=%d)\r\n", (int)eep_st);
        UART_Print(uart_buf);
      }
    }

    // Print status every second — DWT-based (480M cycles = 1s, SysTick-independent)
    if ((DWT->CYCCNT - last_hb_dwt) >= 480000000UL) {
      last_hb_dwt = DWT->CYCCNT;

      // LED now toggled in Half callback at 50Hz for DMA sync visual

      // Calculate callbacks per second
      uint32_t half_per_sec = half_complete_count - last_half;
      uint32_t full_per_sec = full_complete_count - last_full;
      last_half = half_complete_count;
      last_full = full_complete_count;

      // Expected: 50 half + 50 full per second (20ms buffer cycle @ 48kHz TDM8)
      sprintf(uart_buf, "[%lus] Half: %lu/s, Full: %lu/s, Err: %lu, Undr: %lu, RX: %lu bytes\r\n",
              HAL_GetTick() / 1000, half_per_sec, full_per_sec,
              error_count, underrun_count, UART_Protocol_GetRxByteCount());
      UART_Print(uart_buf);

      // Latency stats (cycles → microseconds: cycles / CPU_FREQ_MHZ)
      // CPU @ 480MHz: 1 cycle = 2.08ns, so us = cycles / 480
      if (fill_sample_count > 0) {
        uint32_t fill_avg = (uint32_t)(fill_sum_cycles / fill_sample_count);
        sprintf(uart_buf, "     Fill: min=%lu avg=%lu max=%lu cyc (%lu/%lu/%lu us)\r\n",
                fill_min_cycles, fill_avg, fill_max_cycles,
                fill_min_cycles / CPU_FREQ_MHZ, fill_avg / CPU_FREQ_MHZ, fill_max_cycles / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
      }
      if (jitter_sample_count > 0) {
        uint32_t jitter_avg = (uint32_t)(jitter_sum_cycles / jitter_sample_count);
        uint32_t jitter_range = jitter_max_cycles - jitter_min_cycles;
        sprintf(uart_buf, "     Jitter: period min=%lu avg=%lu max=%lu cyc (range=%lu = %lu us)\r\n",
                jitter_min_cycles, jitter_avg, jitter_max_cycles,
                jitter_range, jitter_range / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
      }

      // LED dimmer: initialize DWT reference on first heartbeat
      if (last_dimmer_dwt == 0) last_dimmer_dwt = DWT->CYCCNT;

      // CPU temperature
      int32_t temp = Read_CPU_Temperature();
      if (temp != -999) {
        int32_t t_abs = temp >= 0 ? temp : -temp;
        sprintf(uart_buf, "     Temp: %ld.%ld C\r\n", temp / 10, t_abs % 10);
        UART_Print(uart_buf);
      }

      // Check SAI status
      HAL_SAI_StateTypeDef sai_state = HAL_SAI_GetState(&hsai_BlockA1);
      if (sai_state != HAL_SAI_STATE_BUSY_TX) {
        sprintf(uart_buf, "WARNING: SAI State = %d (expected BUSY_TX=2)\r\n", sai_state);
        UART_Print(uart_buf);
        sprintf(uart_buf, "SAI Error: 0x%08lX, Last: 0x%08lX\r\n",
                HAL_SAI_GetError(&hsai_BlockA1), last_sai_error);
        UART_Print(uart_buf);
      }

      // 60-second test summary (repeating)
      if (HAL_GetTick() - test_start >= 60000) {
        test_cycle++;
        uint32_t total_time = test_cycle * 60;

        // Snapshot counters BEFORE blocking UART prints
        // (UART printing takes ~50ms total, can cause spurious underrun)
        uint32_t snap_half = half_complete_count;
        uint32_t snap_full = full_complete_count;
        uint32_t snap_err = error_count;
        uint32_t snap_undr = underrun_count;

        // Snapshot latency stats
        uint32_t snap_fill_min = fill_min_cycles;
        uint32_t snap_fill_max = fill_max_cycles;
        uint32_t snap_fill_avg = fill_sample_count > 0
            ? (uint32_t)(fill_sum_cycles / fill_sample_count) : 0;
        uint32_t snap_jitter_min = jitter_min_cycles;
        uint32_t snap_jitter_max = jitter_max_cycles;
        uint32_t snap_jitter_avg = jitter_sample_count > 0
            ? (uint32_t)(jitter_sum_cycles / jitter_sample_count) : 0;

        uint8_t test_passed = (snap_err == 0 && snap_undr == 0
                               && snap_half >= 2900 && snap_full >= 2900);

        UART_Print("\r\n========== 60 SECOND TEST COMPLETE ==========\r\n");
        sprintf(uart_buf, "Cycle #%lu (Total time: %lu s)\r\n", test_cycle, total_time);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Total Half callbacks: %lu (expected ~3000)\r\n", snap_half);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Total Full callbacks: %lu (expected ~3000)\r\n", snap_full);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Total Errors: %lu\r\n", snap_err);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Avg Half rate: %lu/s (expected 50)\r\n", snap_half / 60);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Avg Full rate: %lu/s (expected 50)\r\n", snap_full / 60);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Total Underruns: %lu\r\n", snap_undr);
        UART_Print(uart_buf);

        // Latency characterization report
        uint32_t buf_latency_us = (AUDIO_BUFFER_SAMPLES * 1000) / (AUDIO_SAMPLE_RATE / 1000);
        UART_Print("\r\n--- LATENCY CHARACTERIZATION ---\r\n");
        sprintf(uart_buf, "Buffer: %d samples = %lu us\r\n",
                AUDIO_BUFFER_SAMPLES, buf_latency_us);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Fill min: %lu cyc = %lu us\r\n",
                snap_fill_min, snap_fill_min / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Fill avg: %lu cyc = %lu us\r\n",
                snap_fill_avg, snap_fill_avg / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Fill max: %lu cyc = %lu us\r\n",
                snap_fill_max, snap_fill_max / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Total pipeline: %lu us + %lu us = %lu us\r\n",
                buf_latency_us, snap_fill_avg / CPU_FREQ_MHZ,
                buf_latency_us + snap_fill_avg / CPU_FREQ_MHZ);
        UART_Print(uart_buf);

        UART_Print("\r\n--- JITTER ANALYSIS ---\r\n");
        // Period between consecutive Half (or Full) callbacks = full buffer cycle = 2 × half-buffer
        uint32_t expected_period = (uint32_t)((uint64_t)AUDIO_BUFFER_SAMPLES * 2 * CPU_FREQ_MHZ * 1000000ULL / AUDIO_SAMPLE_RATE);
        sprintf(uart_buf, "Expected period: %lu cyc (%lu us)\r\n",
                expected_period, expected_period / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Measured min:    %lu cyc (%lu us)\r\n",
                snap_jitter_min, snap_jitter_min / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Measured avg:    %lu cyc (%lu us)\r\n",
                snap_jitter_avg, snap_jitter_avg / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        sprintf(uart_buf, "Measured max:    %lu cyc (%lu us)\r\n",
                snap_jitter_max, snap_jitter_max / CPU_FREQ_MHZ);
        UART_Print(uart_buf);
        uint32_t jitter_range = snap_jitter_max - snap_jitter_min;
        sprintf(uart_buf, "Jitter range:    %lu cyc = %lu us\r\n",
                jitter_range, jitter_range / CPU_FREQ_MHZ);
        UART_Print(uart_buf);

        if (test_passed) {
          UART_Print("\r\n>>> TEST PASSED: DMA pipeline clean! <<<\r\n");
        } else {
          UART_Print("\r\n>>> TEST FAILED: Check errors above <<<\r\n");
        }
        UART_Print("==============================================\r\n\r\n");

        // Reset counters + flags + latency stats for next cycle
        // Atomic reset: disable IRQ so ISR can't write between our resets
        __disable_irq();
        need_fill_first_half = 0;
        need_fill_second_half = 0;
        need_fill_1b_first_half = 0;
        need_fill_1b_second_half = 0;
        need_fill_2a_first_half = 0;
        need_fill_2a_second_half = 0;
        need_fill_2b_first_half = 0;
        need_fill_2b_second_half = 0;
        rx_3b_first_half_ready = 0;
        rx_3b_second_half_ready = 0;
        half_complete_count = 0;
        full_complete_count = 0;
        error_count = 0;
        underrun_count = 0;
        last_half = 0;
        last_full = 0;
        // Reset latency tracking
        last_half_isr_cycles = 0;
        last_full_isr_cycles = 0;
        jitter_min_cycles = 0xFFFFFFFF;
        jitter_max_cycles = 0;
        jitter_sum_cycles = 0;
        jitter_sample_count = 0;
        fill_min_cycles = 0xFFFFFFFF;
        fill_max_cycles = 0;
        fill_sum_cycles = 0;
        fill_sample_count = 0;
        __DSB();
        __enable_irq();
        test_start = HAL_GetTick();
      }
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure voltage scaling: VOS0 (480MHz) requires stepping through VOS1 first
   *  VOS1 → enable SYSCFG → VOS0 (overdrive)
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** PLL1: HSE 25MHz / M=5 = 5MHz VCI → N=192 → 960MHz VCO → /P=2 = 480MHz SYSCLK
   *  VCI range: 4-8MHz (RCC_PLL1VCIRANGE_2)
   *  VCO range: wide 192-960MHz (RCC_PLL1VCOWIDE)
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Bus clocks: SYSCLK=480MHz, HCLK=240MHz, APBx=120MHz
   *  HCLK /2 = 240MHz (max for VOS0)
   *  APB1-4 /2 = 120MHz each (I2C1, UART7 derive from APB)
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI1|RCC_PERIPHCLK_SAI2|RCC_PERIPHCLK_UART7;
  PeriphClkInitStruct.PLL3.PLL3M = 5;
  PeriphClkInitStruct.PLL3.PLL3N = 68;
  PeriphClkInitStruct.PLL3.PLL3P = 28;
  PeriphClkInitStruct.PLL3.PLL3Q = 3;
  PeriphClkInitStruct.PLL3.PLL3R = 2;
  PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
  PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 6660;  // For exact 12.288 MHz MCLK (was 6329)
  PeriphClkInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL3;
  PeriphClkInitStruct.Sai23ClockSelection = RCC_SAI23CLKSOURCE_PLL3;
  PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_PLL3;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x307075B1;  // 100kHz SM @ 120MHz APB1 (was 0x00909FCE @ 37.5MHz)
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

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SAI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SAI1_Init(void)
{

  /* USER CODE BEGIN SAI1_Init 0 */

  /* USER CODE END SAI1_Init 0 */

  /* USER CODE BEGIN SAI1_Init 1 */

  /* USER CODE END SAI1_Init 1 */
  hsai_BlockA1.Instance = SAI1_Block_A;
  hsai_BlockA1.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockA1.Init.AudioMode = SAI_MODEMASTER_TX;
  hsai_BlockA1.Init.DataSize = SAI_DATASIZE_24;
  hsai_BlockA1.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockA1.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
  hsai_BlockA1.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockA1.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockA1.Init.NoDivider = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockA1.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockA1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockA1.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
  hsai_BlockA1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockA1.Init.PdmInit.Activation = DISABLE;
  hsai_BlockA1.Init.PdmInit.MicPairsNbr = 1;
  hsai_BlockA1.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
  hsai_BlockA1.FrameInit.FrameLength = 256;           // TDM8: 8 slots × 32 bits
  hsai_BlockA1.FrameInit.ActiveFrameLength = 32;
  hsai_BlockA1.FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
  hsai_BlockA1.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockA1.FrameInit.FSOffset = SAI_FS_FIRSTBIT;
  hsai_BlockA1.SlotInit.FirstBitOffset = 0;
  hsai_BlockA1.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockA1.SlotInit.SlotNumber = 8;                // TDM8: 8 slots (was 2)
  hsai_BlockA1.SlotInit.SlotActive = 0x000000FF;       // All 8 slots active (was 0x03)
  if (HAL_SAI_Init(&hsai_BlockA1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SAI1_Init 2 */

  /* SAI1_B: Slave TX synchronized with SAI1_A (channels 8-15) */
  hsai_BlockB1.Instance = SAI1_Block_B;
  hsai_BlockB1.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockB1.Init.AudioMode = SAI_MODESLAVE_TX;
  hsai_BlockB1.Init.DataSize = SAI_DATASIZE_24;
  hsai_BlockB1.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockB1.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
  hsai_BlockB1.Init.Synchro = SAI_SYNCHRONOUS;
  hsai_BlockB1.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockB1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockB1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockB1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockB1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockB1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockB1.Init.PdmInit.Activation = DISABLE;
  hsai_BlockB1.Init.PdmInit.MicPairsNbr = 1;
  hsai_BlockB1.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
  hsai_BlockB1.FrameInit.FrameLength = 256;
  hsai_BlockB1.FrameInit.ActiveFrameLength = 32;
  hsai_BlockB1.FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
  hsai_BlockB1.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockB1.FrameInit.FSOffset = SAI_FS_FIRSTBIT;
  hsai_BlockB1.SlotInit.FirstBitOffset = 0;
  hsai_BlockB1.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockB1.SlotInit.SlotNumber = 8;
  hsai_BlockB1.SlotInit.SlotActive = 0x000000FF;
  if (HAL_SAI_Init(&hsai_BlockB1) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE END SAI1_Init 2 */

}

/**
  * @brief UART7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART7_Init(void)
{

  /* USER CODE BEGIN UART7_Init 0 */

  /* USER CODE END UART7_Init 0 */

  /* USER CODE BEGIN UART7_Init 1 */

  /* USER CODE END UART7_Init 1 */
  huart7.Instance = UART7;
  huart7.Init.BaudRate = 115200;
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
  if (HAL_UARTEx_SetTxFifoThreshold(&huart7, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart7, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart7) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART7_Init 2 */

  /* USER CODE END UART7_Init 2 */

}

/**
  * @brief UART4 Initialization Function (ESP32 communication)
  * @note  PA0=TX, PA1=RX, 115200 8N1
  */
static void MX_UART4_Init(void)
{
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart4, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart4, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SAI2 Initialization Function (DAC2 + DAC3)
  * @note  SAI2_A: Master TX TDM8, PD13(FS)/PD12(SCK)/PD11(SD)
  *        SAI2_B: Slave TX synced to SAI2_A, PD8(SD only)
  *        Both use PLL3 (same as SAI1) for identical sample rate
  */
static void MX_SAI2_Init(void)
{
  /* SAI2_A: Master TX - identical TDM8 config as SAI1_A */
  hsai_BlockA2.Instance = SAI2_Block_A;
  hsai_BlockA2.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockA2.Init.AudioMode = SAI_MODEMASTER_TX;
  hsai_BlockA2.Init.DataSize = SAI_DATASIZE_24;
  hsai_BlockA2.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockA2.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
  hsai_BlockA2.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockA2.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockA2.Init.NoDivider = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockA2.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockA2.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockA2.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
  hsai_BlockA2.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA2.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA2.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA2.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockA2.Init.PdmInit.Activation = DISABLE;
  hsai_BlockA2.Init.PdmInit.MicPairsNbr = 1;
  hsai_BlockA2.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
  hsai_BlockA2.FrameInit.FrameLength = 256;
  hsai_BlockA2.FrameInit.ActiveFrameLength = 32;
  hsai_BlockA2.FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
  hsai_BlockA2.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockA2.FrameInit.FSOffset = SAI_FS_FIRSTBIT;
  hsai_BlockA2.SlotInit.FirstBitOffset = 0;
  hsai_BlockA2.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockA2.SlotInit.SlotNumber = 8;
  hsai_BlockA2.SlotInit.SlotActive = 0x000000FF;
  if (HAL_SAI_Init(&hsai_BlockA2) != HAL_OK)
  {
    Error_Handler();
  }

  /* SAI2_B: Slave TX - synchronized with SAI2_A (shares SCK/FS internally) */
  hsai_BlockB2.Instance = SAI2_Block_B;
  hsai_BlockB2.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockB2.Init.AudioMode = SAI_MODESLAVE_TX;
  hsai_BlockB2.Init.DataSize = SAI_DATASIZE_24;
  hsai_BlockB2.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockB2.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
  hsai_BlockB2.Init.Synchro = SAI_SYNCHRONOUS;
  hsai_BlockB2.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockB2.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockB2.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockB2.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockB2.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockB2.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockB2.Init.PdmInit.Activation = DISABLE;
  hsai_BlockB2.Init.PdmInit.MicPairsNbr = 1;
  hsai_BlockB2.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
  hsai_BlockB2.FrameInit.FrameLength = 256;
  hsai_BlockB2.FrameInit.ActiveFrameLength = 32;
  hsai_BlockB2.FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
  hsai_BlockB2.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockB2.FrameInit.FSOffset = SAI_FS_FIRSTBIT;
  hsai_BlockB2.SlotInit.FirstBitOffset = 0;
  hsai_BlockB2.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockB2.SlotInit.SlotNumber = 8;
  hsai_BlockB2.SlotInit.SlotActive = 0x000000FF;
  if (HAL_SAI_Init(&hsai_BlockB2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SAI3 Initialization Function (ADC input)
  * @note  SAI3_B: Master RX, I2S stereo (2 channels), PD8(SCK)/PD9(SD)/PD10(FS)
  *        Uses PLL3 via Sai23ClockSelection (shared with SAI2)
  */
static void MX_SAI3_Init(void)
{
  /* SAI3_B: Master RX - ADC input (2 channels) */
  hsai_BlockB3.Instance = SAI3_Block_B;
  hsai_BlockB3.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockB3.Init.AudioMode = SAI_MODEMASTER_RX;
  hsai_BlockB3.Init.DataSize = SAI_DATASIZE_24;
  hsai_BlockB3.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockB3.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
  hsai_BlockB3.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockB3.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockB3.Init.NoDivider = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockB3.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockB3.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockB3.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
  hsai_BlockB3.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockB3.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockB3.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockB3.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockB3.Init.PdmInit.Activation = DISABLE;
  hsai_BlockB3.Init.PdmInit.MicPairsNbr = 1;
  hsai_BlockB3.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;
  hsai_BlockB3.FrameInit.FrameLength = 64;              // 2 slots x 32 bits
  hsai_BlockB3.FrameInit.ActiveFrameLength = 32;
  hsai_BlockB3.FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
  hsai_BlockB3.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockB3.FrameInit.FSOffset = SAI_FS_FIRSTBIT;
  hsai_BlockB3.SlotInit.FirstBitOffset = 0;
  hsai_BlockB3.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockB3.SlotInit.SlotNumber = 2;
  hsai_BlockB3.SlotInit.SlotActive = 0x00000003;
  if (HAL_SAI_Init(&hsai_BlockB3) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_ADC3_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  // ADC3 kernel clock: per_ck (HSI 64MHz default), DIV8 = 8MHz
  RCC_PeriphCLKInitTypeDef adc_clk = {0};
  adc_clk.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  adc_clk.AdcClockSelection = RCC_ADCCLKSOURCE_CLKP;
  if (HAL_RCCEx_PeriphCLKConfig(&adc_clk) != HAL_OK) {
    UART_Print("ADC clock config FAILED\r\n");
    return;
  }

  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV8;
  hadc3.Init.Resolution = ADC_RESOLUTION_16B;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc3.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc3) != HAL_OK) {
    UART_Print("ADC3 init FAILED\r\n");
    return;
  }

  HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);

  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK) {
    UART_Print("ADC3 channel config FAILED\r\n");
  }
}

static int32_t Read_CPU_Temperature(void)
{
  HAL_ADC_Start(&hadc3);
  if (HAL_ADC_PollForConversion(&hadc3, 10) != HAL_OK) {
    return -999;
  }
  uint32_t raw = HAL_ADC_GetValue(&hadc3);
  HAL_ADC_Stop(&hadc3);

  // Factory calibration: TS_CAL1 @30C, TS_CAL2 @110C (16-bit, VREF=3.3V)
  uint16_t ts_cal1 = *((uint16_t *)0x1FF1E820);
  uint16_t ts_cal2 = *((uint16_t *)0x1FF1E840);

  // Linear interpolation, x10 for 0.1C precision (no float)
  int32_t temp_x10 = 300 + (int32_t)(raw - ts_cal1) * 800 / (int32_t)(ts_cal2 - ts_cal1);
  return temp_x10;
}

static void MX_TIM3_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};

  // TIM3 clock = APB1 × 2 = 240MHz
  // Prescaler 239 → counter @ 1MHz, ARR 999 → PWM @ 1kHz, 1000 steps resolution
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 239;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = DIMMER_ARR;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
    UART_Print("TIM3 PWM init FAILED\r\n");
    return;
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK ||
      HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) {
    UART_Print("TIM3 CH3/CH4 config FAILED\r\n");
    return;
  }

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);  // D11 PB0
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);  // D10 PB1
  UART_Print("TIM3 PWM started — LED dimmer active (1kHz, 300 steps, 3s cycle)\r\n");
}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration (SAI1_A) */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  /* DMA1_Stream1_IRQn interrupt configuration (SAI2_A) */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA1_Stream2_IRQn interrupt configuration (SAI2_B) */
  HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
  /* DMA1_Stream3_IRQn interrupt configuration (SAI1_B) */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
  /* DMA1_Stream4_IRQn interrupt configuration (SAI3_B RX) */
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
  /* DMAMUX1_OVR_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMAMUX1_OVR_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMAMUX1_OVR_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ADC_BYPASS_GPIO_Port, ADC_BYPASS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, ADC_OSR_Pin|LED_STATUS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : ADC_BYPASS_Pin */
  GPIO_InitStruct.Pin = ADC_BYPASS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ADC_BYPASS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ADC_OSR_Pin LED_STATUS_Pin */
  GPIO_InitStruct.Pin = ADC_OSR_Pin|LED_STATUS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* NOTE: DAC RST/AMUTEI GPIO init moved to after SAI1_A DMA start (needs MCLK) */

  /*AnalogSwitch Config */
  HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC3, SYSCFG_SWITCH_PC3_CLOSE);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
 * @brief Print string via UART (blocking)
 */
static void UART_Print(const char *str) {
  HAL_UART_Transmit(&huart4, (uint8_t*)str, strlen(str), 100);
}

/**
 * @brief Print DMA configuration details for verification
 */
static void Print_DMA_Config(void) {
  char buf[128];

  UART_Print("\r\n--- DMA1 Stream 0 Configuration ---\r\n");

  // Get DMA1_Stream0 CR register
  uint32_t cr = DMA1_Stream0->CR;

  // Direction: Bits 7:6 (00=P2M, 01=M2P, 10=M2M)
  uint32_t dir = (cr >> 6) & 0x03;
  sprintf(buf, "Direction: %s (0x%02lX)\r\n",
          dir == 0x01 ? "Memory-to-Peripheral [OK]" :
          dir == 0x00 ? "Peripheral-to-Memory" : "Memory-to-Memory",
          dir);
  UART_Print(buf);

  // Circular mode: Bit 8
  uint32_t circ = (cr >> 8) & 0x01;
  sprintf(buf, "Circular Mode: %s\r\n", circ ? "ENABLED [OK]" : "DISABLED");
  UART_Print(buf);

  // Memory increment: Bit 10
  uint32_t minc = (cr >> 10) & 0x01;
  sprintf(buf, "Memory Increment: %s\r\n", minc ? "ENABLED [OK]" : "DISABLED");
  UART_Print(buf);

  // Peripheral increment: Bit 9
  uint32_t pinc = (cr >> 9) & 0x01;
  sprintf(buf, "Periph Increment: %s\r\n", pinc ? "ENABLED" : "DISABLED [OK]");
  UART_Print(buf);

  // Memory data size: Bits 14:13 (00=Byte, 01=HalfWord, 10=Word)
  uint32_t msize = (cr >> 13) & 0x03;
  sprintf(buf, "Memory Data Size: %s (0x%02lX)\r\n",
          msize == 0x02 ? "32-bit Word [OK]" :
          msize == 0x01 ? "16-bit HalfWord" : "8-bit Byte",
          msize);
  UART_Print(buf);

  // Peripheral data size: Bits 12:11
  uint32_t psize = (cr >> 11) & 0x03;
  sprintf(buf, "Periph Data Size: %s (0x%02lX)\r\n",
          psize == 0x02 ? "32-bit Word [OK]" :
          psize == 0x01 ? "16-bit HalfWord" : "8-bit Byte",
          psize);
  UART_Print(buf);

  // Priority: Bits 17:16 (00=Low, 01=Medium, 10=High, 11=VeryHigh)
  uint32_t pl = (cr >> 16) & 0x03;
  const char* prio_str[] = {"Low", "Medium", "High", "Very High"};
  sprintf(buf, "Priority: %s\r\n", prio_str[pl]);
  UART_Print(buf);

  // Interrupts: TCIE (bit 4), HTIE (bit 3), TEIE (bit 2), DMEIE (bit 1)
  uint32_t tcie = (cr >> 4) & 0x01;
  uint32_t htie = (cr >> 3) & 0x01;
  sprintf(buf, "Interrupts: TCIE=%s, HTIE=%s\r\n",
          tcie ? "ON [OK]" : "OFF", htie ? "ON [OK]" : "OFF");
  UART_Print(buf);

  // DMAMUX Configuration
  UART_Print("\r\n--- DMAMUX1 Channel 0 ---\r\n");
  uint32_t dmamux_ccr = DMAMUX1_Channel0->CCR;
  uint32_t req_id = dmamux_ccr & 0xFF;  // Request ID in bits 7:0
  sprintf(buf, "Request ID: %lu (expected 87 for SAI1_A) %s\r\n",
          req_id, req_id == 87 ? "[OK]" : "[MISMATCH!]");
  UART_Print(buf);

  // Buffer address verification
  UART_Print("\r\n--- Memory Addresses ---\r\n");
  uint32_t buf_addr = (uint32_t)audio_buffer;
  sprintf(buf, "Audio Buffer: 0x%08lX ", buf_addr);
  UART_Print(buf);

  // Check if in RAM_D2 (0x30000000 - 0x30047FFF)
  if (buf_addr >= 0x30000000 && buf_addr < 0x30048000) {
    UART_Print("(RAM_D2) [OK - DMA accessible]\r\n");
  } else if (buf_addr >= 0x20000000 && buf_addr < 0x20020000) {
    UART_Print("(DTCMRAM) [ERROR - NOT DMA accessible!]\r\n");
  } else if (buf_addr >= 0x24000000 && buf_addr < 0x24080000) {
    UART_Print("(AXI SRAM) [WARNING - Use MDMA only]\r\n");
  } else {
    UART_Print("(Unknown region)\r\n");
  }

  // DMA peripheral address (SAI1 data register)
  uint32_t par = DMA1_Stream0->PAR;
  sprintf(buf, "SAI1_A DR: 0x%08lX\r\n", par);
  UART_Print(buf);

  // DMA memory address
  uint32_t m0ar = DMA1_Stream0->M0AR;
  sprintf(buf, "DMA M0AR: 0x%08lX\r\n", m0ar);
  UART_Print(buf);

  // Number of data items
  uint32_t ndtr = DMA1_Stream0->NDTR;
  sprintf(buf, "NDTR (items): %lu\r\n", ndtr);
  UART_Print(buf);

  // Stream enabled?
  uint32_t en = cr & 0x01;
  sprintf(buf, "Stream Enabled: %s\r\n", en ? "YES" : "NO");
  UART_Print(buf);

  UART_Print("------------------------------------\r\n\r\n");
}

/**
 * @brief Process ADC half-buffer: deinterleave stereo into delay lines
 * @param rx_buf  Pointer to received interleaved L,R,L,R... samples
 * @param samples Number of mono samples (half of ADC_BUFFER_SAMPLES = 240)
 *
 * Feeds the circular delay lines from ADC input.
 * Must be called BEFORE Fill_Audio_Beamform in the same processing cycle.
 */
static void Process_ADC_Half(int32_t *rx_buf, uint32_t samples) {
  uint32_t wp = delay_write_pos;
  for (uint32_t i = 0; i < samples; i++) {
    delay_line_L[(wp + i) & DELAY_LINE_MASK] = rx_buf[i * ADC_CHANNELS];
    delay_line_R[(wp + i) & DELAY_LINE_MASK] = rx_buf[i * ADC_CHANNELS + 1];
  }
  delay_write_pos = (wp + samples) & DELAY_LINE_MASK;
}

/**
 * @brief Fill TX buffer with beamformed audio from delay line
 * @param buffer    Pointer to TDM8 output buffer (8ch interleaved)
 * @param samples   Number of sample frames to fill (240 per half-buffer)
 * @param ch_offset Global channel offset (0, 8, 16, or 24)
 *
 * For each of the 8 channels in this SAI block, reads the delay line at the
 * per-channel delay offset, applies gain and mute. Single beam (L only) for now.
 */
static void Fill_Audio_Beamform(int32_t *buffer, uint32_t samples, uint32_t ch_offset) {
  uint32_t wp = delay_write_pos;
  uint8_t master = beam_config.mute_global ? 0 : beam_config.master_gain;

  for (uint32_t i = 0; i < samples; i++) {
    for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
      uint32_t gch = ch_offset + ch;
      uint16_t delay = beam_config.delays[gch];
      if (delay > MAX_DELAY_SAMPLES) delay = MAX_DELAY_SAMPLES;

      // Read from delay line: wp points to next write position,
      // so the most recent sample is at wp-1. We want sample that is
      // 'delay' samples old, offset by buffer position.
      uint32_t read_pos = (wp - delay - samples + i) & DELAY_LINE_MASK;
      int32_t sample = delay_line_L[read_pos];

      // Apply per-channel gain (fixed-point: * gain >> 8)
      uint8_t gain = beam_config.gains[gch];
      sample = (sample * gain) >> 8;

      // Apply master gain
      sample = (sample * master) >> 8;

      // Apply per-channel mute
      if (beam_config.mutes & (1UL << gch)) {
        sample = 0;
      }

      buffer[i * AUDIO_CHANNELS + ch] = sample;
    }
  }
}

/**
 * @brief SAI TX Half Complete callback - MINIMAL ISR
 * DMA finished sending first half → CPU can now safely fill it
 * Called every 10ms (half of 20ms buffer cycle)
 *
 * IMPORTANT: No heavy processing here! Just set flag for main loop.
 * ISR budget: ~20 cycles (vs ~50000 if sinf() was called here)
 */
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
  if (hsai->Instance == SAI1_Block_A) {
    uint32_t now = DWT->CYCCNT;


    half_complete_count++;

    // Jitter: measure period between consecutive Half callbacks
    if (last_half_isr_cycles != 0) {
      uint32_t period = now - last_half_isr_cycles;
      // Sanity guard: expected ~9.6M cycles (20ms@480MHz). Reject > 15M (31ms) as stale/reset artifact
      if (period < 15000000) {
        if (period < jitter_min_cycles) jitter_min_cycles = period;
        if (period > jitter_max_cycles) jitter_max_cycles = period;
        jitter_sum_cycles += period;
        jitter_sample_count++;
      }
    }
    last_half_isr_cycles = now;

    // Underrun detection: previous request not yet serviced
    if (need_fill_first_half) {
      underrun_count++;
    }

    need_fill_first_half = 1;
    __DSB();
  }
  else if (hsai->Instance == SAI1_Block_B) {
    if (need_fill_1b_first_half) underrun_count++;
    need_fill_1b_first_half = 1;
    __DSB();
  }
  else if (hsai->Instance == SAI2_Block_A) {
    if (need_fill_2a_first_half) underrun_count++;
    need_fill_2a_first_half = 1;
    __DSB();
  }
  else if (hsai->Instance == SAI2_Block_B) {
    if (need_fill_2b_first_half) underrun_count++;
    need_fill_2b_first_half = 1;
    __DSB();
  }
}

/**
 * @brief SAI TX Full Complete callback - MINIMAL ISR
 * DMA finished sending second half → CPU can now safely fill it
 */
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
  if (hsai->Instance == SAI1_Block_A) {
    uint32_t now = DWT->CYCCNT;


    full_complete_count++;

    // Jitter: measure period between consecutive Full callbacks
    if (last_full_isr_cycles != 0) {
      uint32_t period = now - last_full_isr_cycles;
      // Sanity guard: expected ~9.6M cycles (20ms@480MHz). Reject > 15M (31ms) as stale/reset artifact
      if (period < 15000000) {
        if (period < jitter_min_cycles) jitter_min_cycles = period;
        if (period > jitter_max_cycles) jitter_max_cycles = period;
        jitter_sum_cycles += period;
        jitter_sample_count++;
      }
    }
    last_full_isr_cycles = now;

    if (need_fill_second_half) {
      underrun_count++;
    }

    need_fill_second_half = 1;
    __DSB();
  }
  else if (hsai->Instance == SAI1_Block_B) {
    if (need_fill_1b_second_half) underrun_count++;
    need_fill_1b_second_half = 1;
    __DSB();
  }
  else if (hsai->Instance == SAI2_Block_A) {
    if (need_fill_2a_second_half) underrun_count++;
    need_fill_2a_second_half = 1;
    __DSB();
  }
  else if (hsai->Instance == SAI2_Block_B) {
    if (need_fill_2b_second_half) underrun_count++;
    need_fill_2b_second_half = 1;
    __DSB();
  }
}

/**
 * @brief SAI RX Half Complete callback
 * DMA filled first half of RX buffer → CPU can process it
 */
void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
  if (hsai->Instance == SAI3_Block_B) {
    rx_3b_first_half_ready = 1;
    __DSB();
  }
}

/**
 * @brief SAI RX Full Complete callback
 * DMA filled second half of RX buffer → CPU can process it
 */
void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
  if (hsai->Instance == SAI3_Block_B) {
    rx_3b_second_half_ready = 1;
    __DSB();
  }
}

/**
 * @brief SAI Error callback
 */
void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai) {
  error_count++;
  last_sai_error = HAL_SAI_GetError(hsai);

  // Immediate debug output
  char err_buf[80];
  sprintf(err_buf, "!!! SAI ERROR: 0x%08lX ", last_sai_error);
  HAL_UART_Transmit(&huart4, (uint8_t*)err_buf, strlen(err_buf), 100);

  // Decode error flags
  if (last_sai_error & 0x01) HAL_UART_Transmit(&huart4, (uint8_t*)"OVR ", 4, 10);
  if (last_sai_error & 0x02) HAL_UART_Transmit(&huart4, (uint8_t*)"UDR ", 4, 10);
  if (last_sai_error & 0x20) HAL_UART_Transmit(&huart4, (uint8_t*)"WCKCFG ", 7, 10);
  if (last_sai_error & 0x80) HAL_UART_Transmit(&huart4, (uint8_t*)"DMA ", 4, 10);
  HAL_UART_Transmit(&huart4, (uint8_t*)"\r\n", 2, 10);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
