/**
 * @file pin_definitions.h
 * @brief Pin definitions for Domosonica DS10 - STM32H743VIT6
 *
 * Hardware: STM32H743VIT6 LQFP100
 * Crystal: 25 MHz HSE (Y2 PH0/PH1), 32.768 kHz LSE (Y1 PC14/PC15)
 *
 * Source of truth: DS10_DESIGN_REVIEW.md sections 8 and 11
 * Netlist verified 2026-04-02 (574 nets)
 */

#ifndef __PIN_DEFINITIONS_H
#define __PIN_DEFINITIONS_H

/* ============================================================================
 * SAI1 — Audio Output → DAC0 (U2, 0x98) ch0-7 + DAC1 (U3, 0x9C) ch8-15
 * SAI1_A: Master TX (full pins). SAI1_B: Slave TX sync (SD only).
 * All AF6_SAI1 on port E.
 * ============================================================================ */

#define SAI1_MCLK_PIN           GPIO_PIN_2      // PE2  AF6 → all DACs + AK5538 + JP2
#define SAI1_MCLK_PORT          GPIOE
#define SAI1_MCLK_AF            GPIO_AF6_SAI1

#define SAI1_FS_A_PIN           GPIO_PIN_4      // PE4  AF6 SAI1_FS_A  → U2+U3
#define SAI1_FS_A_PORT          GPIOE
#define SAI1_SCK_A_PIN          GPIO_PIN_5      // PE5  AF6 SAI1_SCK_A → U2+U3
#define SAI1_SCK_A_PORT         GPIOE
#define SAI1_SD_A_PIN           GPIO_PIN_6      // PE6  AF6 SAI1_SD_A  → U2 (DAC0)
#define SAI1_SD_A_PORT          GPIOE
#define SAI1_A_AF               GPIO_AF6_SAI1

#define SAI1_SD_B_PIN           GPIO_PIN_3      // PE3  AF6 SAI1_SD_B  → U3 (DAC1)
#define SAI1_SD_B_PORT          GPIOE
#define SAI1_B_AF               GPIO_AF6_SAI1

/* ============================================================================
 * SAI2 — Audio Output → DAC2 (U1, 0x9A) ch16-23 + DAC3 (U4, 0x9E) ch24-31
 * SAI2_A: Master TX (full pins). SAI2_B: Slave TX sync (SD only).
 * AF10_SAI2.
 * ============================================================================ */

#define SAI2_FS_A_PIN           GPIO_PIN_12     // PD12 AF10 SAI2_FS_A  → U1+U4
#define SAI2_FS_A_PORT          GPIOD
#define SAI2_SCK_A_PIN          GPIO_PIN_13     // PD13 AF10 SAI2_SCK_A → U1+U4
#define SAI2_SCK_A_PORT         GPIOD
#define SAI2_SD_A_PIN           GPIO_PIN_11     // PD11 AF10 SAI2_SD_A  → U1 (DAC2)
#define SAI2_SD_A_PORT          GPIOD
#define SAI2_A_AF               GPIO_AF10_SAI2

#define SAI2_SD_B_PIN           GPIO_PIN_11     // PE11 AF10 SAI2_SD_B  → U4 (DAC3)
#define SAI2_SD_B_PORT          GPIOE
#define SAI2_B_AF               GPIO_AF10_SAI2

/* ============================================================================
 * SAI3_B — Audio Input (AK5538VN IC3, addr 0x14) — 8ch TDM256 slave
 * STM32 SAI3_B = Master RX (generates BICK/LRCK). AK5538 = slave (MSN=GND).
 * AF6_SAI3 on port D.
 * ============================================================================ */

#define SAI3_SCK_B_PIN          GPIO_PIN_8      // PD8  AF6 SAI3_SCK_B → IC3/33 BICK
#define SAI3_SCK_B_PORT         GPIOD
#define SAI3_SD_B_PIN           GPIO_PIN_9      // PD9  AF6 SAI3_SD_B  ← IC3/36 SDTO1
#define SAI3_SD_B_PORT          GPIOD
#define SAI3_FS_B_PIN           GPIO_PIN_10     // PD10 AF6 SAI3_FS_B  → IC3/34 LRCK
#define SAI3_FS_B_PORT          GPIOD
#define SAI3_B_AF               GPIO_AF6_SAI3

/* ============================================================================
 * I2C1 — DAC Configuration Bus (4× PCM1690 U1-U4)
 * Pull-ups: RN5 4.7kΩ to +3V3
 * ============================================================================ */

#define I2C1_SCL_PIN            GPIO_PIN_8      // PB8  AF4 I2C1_SCL
#define I2C1_SCL_PORT           GPIOB
#define I2C1_SDA_PIN            GPIO_PIN_9      // PB9  AF4 I2C1_SDA
#define I2C1_SDA_PORT           GPIOB
#define I2C1_AF                 GPIO_AF4_I2C1

/* DS10 DAC I2C addresses (8-bit write, verified against netlist ADR0/ADR1 strapping)
 * U2: ADR0=GND ADR1=GND → 0x4C → 0x98 write → SAI1_SD_A (ch 0-7)
 * U3: ADR0=GND ADR1=+3V3→ 0x4E → 0x9C write → SAI1_SD_B (ch 8-15)
 * U1: ADR0=+3V3 ADR1=GND→ 0x4D → 0x9A write → SAI2_SD_A (ch 16-23)
 * U4: ADR0=+3V3 ADR1=+3V3→0x4F → 0x9E write → SAI2_SD_B (ch 24-31) */
#define PCM1690_DAC0_ADDR       0x98    // U2 → SAI1_SD_A ch0-7
#define PCM1690_DAC1_ADDR       0x9C    // U3 → SAI1_SD_B ch8-15
#define PCM1690_DAC2_ADDR       0x9A    // U1 → SAI2_SD_A ch16-23
#define PCM1690_DAC3_ADDR       0x9E    // U4 → SAI2_SD_B ch24-31

/* ============================================================================
 * I2C2 — Expansion Header J5
 * Pull-ups: RN6 4.7kΩ to +3V3
 * ============================================================================ */

#define I2C2_SCL_PIN            GPIO_PIN_10     // PB10 AF4 I2C2_SCL → J5/1
#define I2C2_SCL_PORT           GPIOB
#define I2C2_SDA_PIN            GPIO_PIN_11     // PB11 AF4 I2C2_SDA → J5/2
#define I2C2_SDA_PORT           GPIOB
#define I2C2_AF                 GPIO_AF4_I2C2

/* ============================================================================
 * I2C3 — AK5538VN ADC (IC3)
 * Pull-ups: RN6 4.7kΩ to +3V3
 * ============================================================================ */

#define I2C3_SCL_PIN            GPIO_PIN_8      // PA8  AF4 I2C3_SCL → IC3/45
#define I2C3_SCL_PORT           GPIOA
#define I2C3_SDA_PIN            GPIO_PIN_9      // PC9  AF4 I2C3_SDA → IC3/43
#define I2C3_SDA_PORT           GPIOC
#define I2C3_AF                 GPIO_AF4_I2C3

#define AK5538_ADDR             0x28    // 8-bit write (7-bit 0x14, I2C enable pin=+3V3)

/* ============================================================================
 * I2C4 — EEPROM (CAT24C256 U8, 32KB)
 * Pull-ups: RN5 4.7kΩ to +3V3
 * ============================================================================ */

#define EEPROM_I2C_SCL_PIN      GPIO_PIN_6      // PB6  AF11 I2C4_SCL → U8/6
#define EEPROM_I2C_SCL_PORT     GPIOB
#define EEPROM_I2C_SDA_PIN      GPIO_PIN_7      // PB7  AF11 I2C4_SDA → U8/5
#define EEPROM_I2C_SDA_PORT     GPIOB
#define EEPROM_I2C_AF           GPIO_AF11_I2C4

#define EEPROM_ADDR             0xA0    // 8-bit write (A0=A1=A2=GND → 7-bit 0x50)
#define EEPROM_ADDR_7BIT        0x50

#define EEPROM_WP_PIN           GPIO_PIN_5      // PB5  — HIGH=write-protected
#define EEPROM_WP_PORT          GPIOB

/* ============================================================================
 * UART4 — ESP32-C3-Mini (U5) communication
 * PA0=STM32_TX → U5/13(ESP32_RX), PA1=STM32_RX ← U5/12(ESP32_TX)
 * ============================================================================ */

#define UART4_TX_PIN            GPIO_PIN_0      // PA0  AF8 UART4_TX → ESP32
#define UART4_TX_PORT           GPIOA
#define UART4_RX_PIN            GPIO_PIN_1      // PA1  AF8 UART4_RX ← ESP32
#define UART4_RX_PORT           GPIOA
#define UART4_AF                GPIO_AF8_UART4

/* ============================================================================
 * UART7 — Debug (J7 header pins 7/8, alongside SWD)
 * ============================================================================ */

#define UART7_TX_PIN            GPIO_PIN_8      // PE8  AF7 UART7_TX → J7/8
#define UART7_TX_PORT           GPIOE
#define UART7_RX_PIN            GPIO_PIN_7      // PE7  AF7 UART7_RX ← J7/7
#define UART7_RX_PORT           GPIOE
#define UART7_AF                GPIO_AF7_UART7

/* ============================================================================
 * DAC Shared Control (all 4 PCM1690 share RST and AMUTEI/AMUTEO)
 * DS10 eliminates per-chip GPIO — RST and MUTE controlled as a bus.
 * DAC_RST:      PC11 pin79 — active LOW. Hold LOW during init, release HIGH.
 * DAC_MUTE_IN:  PC12 pin80 — AMUTEI. LOW=muted (safe), HIGH=unmuted.
 * DAC_MUTE_OUT: PC10 pin78 — AMUTEO wire-OR from all 4 DACs (input).
 * ============================================================================ */

#define DAC_RST_PIN             GPIO_PIN_11     // PC11 — ~RST all 4 PCM1690
#define DAC_RST_PORT            GPIOC
#define DAC_MUTE_IN_PIN         GPIO_PIN_12     // PC12 — AMUTEI all 4 PCM1690
#define DAC_MUTE_IN_PORT        GPIOC
#define DAC_MUTE_OUT_PIN        GPIO_PIN_10     // PC10 — AMUTEO wire-OR (input)
#define DAC_MUTE_OUT_PORT       GPIOC

/* ============================================================================
 * PAM8302A Amplifier Bank Control (32× amplifiers)
 * PD3 → R19(pull-down) + R20(series) → /ALL_AMPS/SD
 * LOW = shutdown (safe during reset), HIGH = amps active
 * ============================================================================ */

#define PAM_SD_PIN              GPIO_PIN_3      // PD3  — amp shutdown (ALL 32)
#define PAM_SD_PORT             GPIOD

/* ============================================================================
 * AK5538VN ADC Control
 * PDN:  PC7 — power-down (active LOW: HIGH=normal, LOW=active)
 * DIF1: PC8 — interface select (strapped LOW via hardware, mirror in fw)
 * OVF:  PA15— overflow flag output from AK5538 (input to STM32)
 * ============================================================================ */

#define AK_PDN_PIN              GPIO_PIN_7      // PC7  — AK5538 power-down
#define AK_PDN_PORT             GPIOC
#define AK_DIF1_PIN             GPIO_PIN_8      // PC8  — AK5538 DIF1
#define AK_DIF1_PORT            GPIOC
#define AK_OVF_PIN              GPIO_PIN_15     // PA15 — AK5538 overflow (input)
#define AK_OVF_PORT             GPIOA

/* ============================================================================
 * Status LEDs — TIM3 PWM (AF2)
 * D4: PA6 TIM3_CH1 Yellow
 * D5: PA7 TIM3_CH2 Red
 * D6: PB0 TIM3_CH3 Green
 * D7: PB1 TIM3_CH4 White
 * ============================================================================ */

#define LED_D4_PIN              GPIO_PIN_6      // PA6  AF2 TIM3_CH1 (Yellow)
#define LED_D4_PORT             GPIOA
#define LED_D5_PIN              GPIO_PIN_7      // PA7  AF2 TIM3_CH2 (Red)
#define LED_D5_PORT             GPIOA
#define LED_D6_PIN              GPIO_PIN_0      // PB0  AF2 TIM3_CH3 (Green)
#define LED_D6_PORT             GPIOB
#define LED_D7_PIN              GPIO_PIN_1      // PB1  AF2 TIM3_CH4 (White)
#define LED_D7_PORT             GPIOB

/* Semantic aliases */
#define LED_POWER_PIN           LED_D4_PIN
#define LED_POWER_PORT          LED_D4_PORT
#define LED_AUDIO_PIN           LED_D5_PIN
#define LED_AUDIO_PORT          LED_D5_PORT
#define LED_COMM_PIN            LED_D6_PIN
#define LED_COMM_PORT           LED_D6_PORT
#define LED_STATUS_PIN          LED_D7_PIN
#define LED_STATUS_PORT         LED_D7_PORT

/* ============================================================================
 * Buttons / Switches
 * ============================================================================ */

#define OPT_SW_K2_PIN           GPIO_PIN_4      // PA4  — user button K2
#define OPT_SW_K2_PORT          GPIOA

/* ============================================================================
 * SWD / Debug (J7 header)
 * ============================================================================ */

#define SWDIO_PIN               GPIO_PIN_13     // PA13 → J7/2
#define SWDIO_PORT              GPIOA
#define SWCLK_PIN               GPIO_PIN_14     // PA14 → J7/3
#define SWCLK_PORT              GPIOA
#define SWO_PIN                 GPIO_PIN_3      // PB3  → J7/5
#define SWO_PORT                GPIOB

/* ============================================================================
 * Audio Clock Configuration
 * PLL3: M=5 N=68 P=14 FRACN=6660 → 24.576 MHz (512×fS) on PE2 SAI1_MCLK
 * MckOverSampling=ENABLE on SAI1_A and SAI2_A → MCLK pin = 2×BCK = 24.576 MHz
 * BCK = 12.288 MHz (256×32bit×48kHz), FS = 48kHz
 * PCM1690: 512×fS MCLK → single-rate mode → 8ch per DIN1
 * AK5538VN: receives BICK=12.288MHz, LRCK=48kHz from SAI3_B master
 * ============================================================================ */

#define AUDIO_SAMPLE_RATE       48000
#define AUDIO_MCLK_FREQ         24576000    // 512×fS on PE2 (MckOverSampling=ENABLE)
#define AUDIO_BCK_FREQ          12288000    // 256×fS (8 slots × 32 bits × 48kHz)
#define AUDIO_FS_FREQ           48000

/* ============================================================================
 * Channel Configuration
 * ============================================================================ */

#define AUDIO_DAC_COUNT         4           // 4× PCM1690
#define AUDIO_CHANNELS_PER_DAC  8           // 8ch per PCM1690
#define AUDIO_TOTAL_CHANNELS    32          // Total output channels
#define AUDIO_ADC_CHANNELS      8           // AK5538VN: 8 input channels

/* ============================================================================
 * DS10 STM32 Pin Usage Summary
 *
 * PA0  = UART4_TX      PA8  = I2C3_SCL      PB0  = LED_D6 TIM3_CH3
 * PA1  = UART4_RX      PA9  = [analog]      PB1  = LED_D7 TIM3_CH4
 * PA2  = USART2_TX     PA10 = [analog]      PB3  = SWO
 * PA3  = USART2_RX     PA11 = USB_DM        PB5  = EEPROM_WP
 * PA4  = OPT_SW_K2     PA12 = USB_DP        PB6  = I2C4_SCL (AF11)
 * PA5  = [analog]      PA13 = SWDIO         PB7  = I2C4_SDA (AF11)
 * PA6  = LED_D4 CH1    PA14 = SWCLK         PB8  = I2C1_SCL
 * PA7  = LED_D5 CH2    PA15 = AK_OVF(in)    PB9  = I2C1_SDA
 *                                           PB10 = I2C2_SCL
 * PC7  = AK_PDN        PD3  = PAM_SD        PB11 = I2C2_SDA
 * PC8  = AK_DIF1       PD8  = SAI3_SCK_B
 * PC9  = I2C3_SDA      PD9  = SAI3_SD_B
 * PC10 = DAC_MUTE_OUT  PD10 = SAI3_FS_B
 * PC11 = DAC_RST       PD11 = SAI2_SD_A
 * PC12 = DAC_MUTE_IN   PD12 = SAI2_FS_A
 *                      PD13 = SAI2_SCK_A
 * PE2  = SAI1_MCLK
 * PE3  = SAI1_SD_B     PE7  = UART7_RX
 * PE4  = SAI1_FS_A     PE8  = UART7_TX
 * PE5  = SAI1_SCK_A    PE11 = SAI2_SD_B
 * PE6  = SAI1_SD_A
 *
 * Analog (unused, configured GPIO_MODE_ANALOG to minimize EMI):
 * PE0,PE1,PE9,PE10,PE12-PE15,PB2,PB4,PB15,PC0,PC1,PC2,PC3,PC6,
 * PD2,PD5,PD6,PD14,PD15,PA5,PA9,PA10
 * ============================================================================ */

#endif /* __PIN_DEFINITIONS_H */
