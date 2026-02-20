/**
 * @file pin_definitions_ds10.h
 * @brief CORRECTED pin definitions for Domosonica DS10 - STM32H743VIT6
 *
 * Hardware: STM32H743VIT6 LQFP100
 * Crystal: 25 MHz HSE (Y2), 32.768 kHz LSE (Y1)
 * Version: DS10 - Corrected from DS9 v2.0 pin audit (Feb 2026)
 *
 * DS9 errors fixed:
 *   - SAI1_A: PE7/PE8/PE9 had NO SAI function → moved to PE4/PE5/PE6
 *   - SAI1_B: Uses sync mode (PE3 SD only) → was incorrectly PE4/PE5/PE6
 *   - SAI2_A: PD12↔PD13 SCK/FS labels were swapped → corrected
 *   - SAI2_B: PD8/PD9/PD10 were SAI3_B not SAI2_B → moved to PE11 sync
 *   - ADC: SAI4_B SCK/FS not on LQFP100 → use SAI3_B on PD8/PD9/PD10
 *   - Pin conflicts resolved: PE3(RST→moved), PE11(AMUTEI→moved)
 *
 * AF mapping verified against DS12110 Rev 10 Tables 13-14 (pages 93-94)
 */

#ifndef __PIN_DEFINITIONS_H
#define __PIN_DEFINITIONS_H

/* ============================================================================
 * SAI1 - Audio Output (DAC0, DAC1) - Channels 0-15
 * SAI1_A: Master TX (DAC0) - Full pins (FS, SCK, SD)
 * SAI1_B: Slave TX sync (DAC1) - SD only, shares SCK/FS from SAI1_A
 * AF6_SAI1 on all pins
 * ============================================================================ */

// SAI1 Master Clock (shared by all audio chips via 33 Ohm series resistor)
#define SAI1_MCLK_PIN           GPIO_PIN_2
#define SAI1_MCLK_PORT          GPIOE
#define SAI1_MCLK_AF            GPIO_AF6_SAI1

// SAI1_A (DAC0 - U3 PCM1690) - Channels 0-7, Master TX
#define SAI1_FS_A_PIN           GPIO_PIN_4      // PE4 - Frame Sync (AF6=SAI1_FS_A)
#define SAI1_FS_A_PORT          GPIOE
#define SAI1_SCK_A_PIN          GPIO_PIN_5      // PE5 - Bit Clock (AF6=SAI1_SCK_A)
#define SAI1_SCK_A_PORT         GPIOE
#define SAI1_SD_A_PIN           GPIO_PIN_6      // PE6 - Serial Data (AF6=SAI1_SD_A)
#define SAI1_SD_A_PORT          GPIOE
#define SAI1_A_AF               GPIO_AF6_SAI1

// SAI1_B (DAC1 - U4 PCM1690) - Channels 8-15, Slave TX (synchronous with SAI1_A)
// Sync mode: SCK and FS shared internally from SAI1_A, only SD pin needed
// DAC1 receives BCK/LRCK from same bus as DAC0 (physical PCB connection)
#define SAI1_SD_B_PIN           GPIO_PIN_3      // PE3 - Serial Data (AF6=SAI1_SD_B)
#define SAI1_SD_B_PORT          GPIOE
#define SAI1_B_AF               GPIO_AF6_SAI1
// Note: SAI1_SCK_B(PF8) and SAI1_FS_B(PF9) NOT available on LQFP100
// SAI1_B MUST use synchronous mode on this package

/* ============================================================================
 * SAI2 - Audio Output (DAC2, DAC3) - Channels 16-31
 * SAI2_A: Master TX (DAC2) - Full pins (FS, SCK, SD)
 * SAI2_B: Slave TX sync (DAC3) - SD only, shares SCK/FS from SAI2_A
 * AF10_SAI2 on all pins
 * ============================================================================ */

// SAI2_A (DAC2 - U2 PCM1690) - Channels 16-23, Master TX
// IMPORTANT: PD12=FS, PD13=SCK (DS9 had these labels swapped!)
#define SAI2_FS_A_PIN           GPIO_PIN_12     // PD12 - Frame Sync (AF10=SAI2_FS_A)
#define SAI2_FS_A_PORT          GPIOD
#define SAI2_SCK_A_PIN          GPIO_PIN_13     // PD13 - Bit Clock (AF10=SAI2_SCK_A)
#define SAI2_SCK_A_PORT         GPIOD
#define SAI2_SD_A_PIN           GPIO_PIN_11     // PD11 - Serial Data (AF10=SAI2_SD_A)
#define SAI2_SD_A_PORT          GPIOD
#define SAI2_A_AF               GPIO_AF10_SAI2

// SAI2_B (DAC3 - U8 PCM1690) - Channels 24-31, Slave TX (synchronous with SAI2_A)
// Sync mode: SCK and FS shared internally from SAI2_A, only SD pin needed
// DAC3 receives BCK/LRCK from same bus as DAC2 (physical PCB connection)
#define SAI2_SD_B_PIN           GPIO_PIN_11     // PE11 - Serial Data (AF10=SAI2_SD_B)
#define SAI2_SD_B_PORT          GPIOE
#define SAI2_B_AF               GPIO_AF10_SAI2
// Note: PE12=SAI2_SCK_B, PE13=SAI2_FS_B available but not needed in sync mode

/* ============================================================================
 * SAI3 - Audio Input (ADC - U1 PCM1802) - 2 Channels
 * SAI3_B: Master RX or Slave RX depending on PCM1802 mode
 * AF6_SAI3 on PD8/PD9/PD10
 * DS9 had these pins assigned as SAI2_B (wrong!) — now correctly SAI3_B
 * ============================================================================ */

// SAI3_B (ADC - U1 PCM1802) - 2 channels input
#define SAI3_SCK_B_PIN          GPIO_PIN_8      // PD8 - Bit Clock (AF6=SAI3_SCK_B)
#define SAI3_SCK_B_PORT         GPIOD
#define SAI3_SD_B_PIN           GPIO_PIN_9      // PD9 - Serial Data In (AF6=SAI3_SD_B)
#define SAI3_SD_B_PORT          GPIOD
#define SAI3_FS_B_PIN           GPIO_PIN_10     // PD10 - Frame Sync (AF6=SAI3_FS_B)
#define SAI3_FS_B_PORT          GPIOD
#define SAI3_B_AF               GPIO_AF6_SAI3

/* ============================================================================
 * I2C1 - DAC Configuration Bus (4x PCM1690)
 * Pull-ups: 5.1k (optimal: 4.7k for 400kHz)
 * ============================================================================ */

#define I2C1_SCL_PIN            GPIO_PIN_8      // PB8
#define I2C1_SCL_PORT           GPIOB
#define I2C1_SDA_PIN            GPIO_PIN_9      // PB9
#define I2C1_SDA_PORT           GPIOB
#define I2C1_AF                 GPIO_AF4_I2C1

// PCM1690 I2C Addresses (8-bit write addresses from datasheet)
#define PCM1690_DAC0_ADDR       0x94    // U3 - ADR1=0, ADR0=0 - Channels 0-7
#define PCM1690_DAC1_ADDR       0x96    // U4 - ADR1=0, ADR0=1 - Channels 8-15
#define PCM1690_DAC2_ADDR       0x98    // U2 - ADR1=1, ADR0=0 - Channels 16-23
#define PCM1690_DAC3_ADDR       0x9A    // U8 - ADR1=1, ADR0=1 - Channels 24-31

// 7-bit addresses for HAL functions that require 7-bit
#define PCM1690_DAC0_ADDR_7BIT  0x4A    // 0x94 >> 1
#define PCM1690_DAC1_ADDR_7BIT  0x4B    // 0x96 >> 1
#define PCM1690_DAC2_ADDR_7BIT  0x4C    // 0x98 >> 1
#define PCM1690_DAC3_ADDR_7BIT  0x4D    // 0x9A >> 1

/* ============================================================================
 * I2C3 - EEPROM Bus (CAT24C256 - U6)
 * Pull-ups: 5.1k (optimal: 4.7k for 400kHz)
 * ============================================================================ */

#define I2C3_SCL_PIN            GPIO_PIN_6      // PB6
#define I2C3_SCL_PORT           GPIOB
#define I2C3_SDA_PIN            GPIO_PIN_7      // PB7
#define I2C3_SDA_PORT           GPIOB
#define I2C3_AF                 GPIO_AF4_I2C3

// EEPROM CAT24C256 (U6) - 32KB
#define EEPROM_ADDR             0xA0    // 8-bit address (A0-A2 = GND)
#define EEPROM_ADDR_7BIT        0x50    // 7-bit address

// EEPROM Write Protect
#define EEPROM_WP_PIN           GPIO_PIN_5      // PB5
#define EEPROM_WP_PORT          GPIOB
// WP = LOW: Write enabled, WP = HIGH: Write protected

/* ============================================================================
 * UART4 - ESP32-C3 Communication
 * Baud: 115200 (or up to 921600)
 * Format: 8N1
 * ============================================================================ */

#define UART4_TX_PIN            GPIO_PIN_0      // PA0 -> ESP32 GPIO3 (RX)
#define UART4_TX_PORT           GPIOA
#define UART4_RX_PIN            GPIO_PIN_1      // PA1 <- ESP32 GPIO2 (TX)
#define UART4_RX_PORT           GPIOA
#define UART4_AF                GPIO_AF8_UART4

/* ============================================================================
 * USB Type-C (J3) - Programming/Debug
 * ============================================================================ */

#define USB_DM_PIN              GPIO_PIN_11     // PA11 - USB D-
#define USB_DM_PORT             GPIOA
#define USB_DP_PIN              GPIO_PIN_12     // PA12 - USB D+
#define USB_DP_PORT             GPIOA
#define USB_AF                  GPIO_AF10_OTG1_FS

/* ============================================================================
 * DAC Control Pins - Reset (Active LOW)
 * Sequence: Hold all LOW, start MCLK, release one at a time with 1ms delay
 *
 * DS10 changes:
 *   - DAC1_RST moved from PE3 to PC8 (PE3 now SAI1_SD_B)
 * ============================================================================ */

#define DAC0_RST_PIN            GPIO_PIN_10     // PE10 - U3 Reset
#define DAC0_RST_PORT           GPIOE
#define DAC1_RST_PIN            GPIO_PIN_8      // PC8 - U4 Reset (was PE3 in DS9, conflicts SAI1_SD_B)
#define DAC1_RST_PORT           GPIOC
#define DAC2_RST_PIN            GPIO_PIN_10     // PC10 - U2 Reset
#define DAC2_RST_PORT           GPIOC
#define DAC3_RST_PIN            GPIO_PIN_15     // PB15 - U8 Reset
#define DAC3_RST_PORT           GPIOB

/* ============================================================================
 * DAC Control Pins - Mute Input/Output
 * AMUTEI: Active LOW (LOW = muted, HIGH = unmuted)
 * AMUTEO: Status output from DAC
 *
 * DS10 changes:
 *   - DAC0_AMUTEI moved from PE11 to PE7 (PE11 now SAI2_SD_B)
 *   - DAC0_AMUTEO moved from PE12 to PE8 (PE12 freed)
 * ============================================================================ */

// DAC0 (U3) Mute - moved from PE11/PE12 (now SAI2_B)
#define DAC0_AMUTEI_PIN         GPIO_PIN_7      // PE7 - Mute Input (was PE11 in DS9)
#define DAC0_AMUTEI_PORT        GPIOE
#define DAC0_AMUTEO_PIN         GPIO_PIN_8      // PE8 - Mute Output (was PE12 in DS9)
#define DAC0_AMUTEO_PORT        GPIOE

// DAC1 (U4) Mute - unchanged
#define DAC1_AMUTEI_PIN         GPIO_PIN_1      // PE1 - Mute Input
#define DAC1_AMUTEI_PORT        GPIOE
#define DAC1_AMUTEO_PIN         GPIO_PIN_0      // PE0 - Mute Output (status)
#define DAC1_AMUTEO_PORT        GPIOE

// DAC2 (U2) Mute - unchanged
#define DAC2_AMUTEI_PIN         GPIO_PIN_11     // PC11 - Mute Input
#define DAC2_AMUTEI_PORT        GPIOC
#define DAC2_AMUTEO_PIN         GPIO_PIN_12     // PC12 - Mute Output (status)
#define DAC2_AMUTEO_PORT        GPIOC

// DAC3 (U8) Mute - unchanged
#define DAC3_AMUTEI_PIN         GPIO_PIN_14     // PB14 - Mute Input
#define DAC3_AMUTEI_PORT        GPIOB
#define DAC3_AMUTEO_PIN         GPIO_PIN_13     // PB13 - Mute Output (status)
#define DAC3_AMUTEO_PORT        GPIOB

/* ============================================================================
 * ADC Control Pins (PCM1802 - U1)
 * ============================================================================ */

// Power and basic control
#define ADC_PDWN_PIN            GPIO_PIN_7      // PA7 - Power Down (HIGH=normal)
#define ADC_PDWN_PORT           GPIOA
#define ADC_OSR_PIN             GPIO_PIN_5      // PA5 - Oversampling (HIGH=128x)
#define ADC_OSR_PORT            GPIOA
#define ADC_BYPASS_PIN          GPIO_PIN_6      // PA6 - HPF Bypass (HIGH=bypass)
#define ADC_BYPASS_PORT         GPIOA
#define ADC_FSYNC_PIN           GPIO_PIN_4      // PA4 - Frame Sync Input
#define ADC_FSYNC_PORT          GPIOA

// Format selection (FMT1:FMT0)
// 00=I2S, 01=Left-Justified, 10=Right-Just 24-bit, 11=Right-Just 16-bit
#define ADC_FMT0_PIN            GPIO_PIN_7      // PC7 - Format bit 0
#define ADC_FMT0_PORT           GPIOC
#define ADC_FMT1_PIN            GPIO_PIN_6      // PC6 - Format bit 1
#define ADC_FMT1_PORT           GPIOC

// Mode selection (MODE1:MODE0)
// 00=Slave, 01=Master 256fs, 10=Master 384fs, 11=Master 512fs
#define ADC_MODE0_PIN           GPIO_PIN_14     // PD14 - Mode bit 0
#define ADC_MODE0_PORT          GPIOD
#define ADC_MODE1_PIN           GPIO_PIN_15     // PD15 - Mode bit 1
#define ADC_MODE1_PORT          GPIOD

/* ============================================================================
 * Status LEDs (Active HIGH)
 * ============================================================================ */

#define LED1_PIN                GPIO_PIN_4      // PC4 - D6
#define LED1_PORT               GPIOC
#define LED2_PIN                GPIO_PIN_5      // PC5 - D7
#define LED2_PORT               GPIOC
#define LED3_PIN                GPIO_PIN_0      // PB0 - D11
#define LED3_PORT               GPIOB
#define LED4_PIN                GPIO_PIN_1      // PB1 - D10
#define LED4_PORT               GPIOB

#define LED_POWER_PIN           LED1_PIN
#define LED_POWER_PORT          LED1_PORT
#define LED_AUDIO_PIN           LED2_PIN
#define LED_AUDIO_PORT          LED2_PORT
#define LED_COMM_PIN            LED3_PIN
#define LED_COMM_PORT           LED3_PORT
#define LED_STATUS_PIN          LED4_PIN
#define LED_STATUS_PORT         LED4_PORT

/* ============================================================================
 * Power Control
 * ============================================================================ */

#define PWR_EN_3V3A_PIN         GPIO_PIN_10     // PA10 - Enable 3.3V analog
#define PWR_EN_3V3A_PORT        GPIOA

/* ============================================================================
 * Test Points
 * DS10: PE13 freed (was TP1, now available since SAI2_B sync doesn't need FS pin)
 * ============================================================================ */

#define TP1_PIN                 GPIO_PIN_9      // PE9 - General debug (was PE13, now freed)
#define TP1_PORT                GPIOE
#define TP2_PIN                 GPIO_PIN_14     // PE14 - General debug
#define TP2_PORT                GPIOE
#define TP3_PIN                 GPIO_PIN_8      // PA8 - General debug
#define TP3_PORT                GPIOA

/* ============================================================================
 * SWD Debug Interface (Reserved)
 * ============================================================================ */

#define SWDIO_PIN               GPIO_PIN_13     // PA13
#define SWDIO_PORT              GPIOA
#define SWCLK_PIN               GPIO_PIN_14     // PA14
#define SWCLK_PORT              GPIOA

/* ============================================================================
 * Crystals (Hardware - not GPIO configurable)
 * ============================================================================ */

// Y1 - LSE 32.768 kHz for RTC
// PC14 (OSC32_IN), PC15 (OSC32_OUT)

// Y2 - HSE 25 MHz Main Clock
// PH0 (OSC_IN), PH1 (OSC_OUT)

/* ============================================================================
 * Audio Clock Configuration
 * MCLK = 12.288 MHz (256 x 48kHz)
 * Generated from PLL3 via SAI1_MCLK (PE2)
 * PLL3 feeds SAI1, SAI2, SAI3 via Sai23ClockSelection
 * ============================================================================ */

#define AUDIO_SAMPLE_RATE       48000
#define AUDIO_MCLK_FREQ         12288000    // 256 x fs
#define AUDIO_BCK_FREQ          3072000     // 64 x fs (TDM8: 8 slots x 32 bits)
#define AUDIO_FS_FREQ           48000       // Sample rate

/* ============================================================================
 * Channel Configuration
 * ============================================================================ */

#define AUDIO_DAC_COUNT         4           // Number of PCM1690 chips
#define AUDIO_CHANNELS_PER_DAC  8           // Channels per PCM1690
#define AUDIO_TOTAL_CHANNELS    32          // Total output channels
#define AUDIO_ADC_CHANNELS      2           // Input channels (PCM1802)

/* ============================================================================
 * DS10 Pin Usage Summary (for PCB routing verification)
 *
 * PE0  = DAC1_AMUTEO       PE8  = DAC0_AMUTEO     PB0  = LED3
 * PE1  = DAC1_AMUTEI       PE9  = TP1             PB1  = LED4
 * PE2  = SAI1_MCLK (AF6)   PE10 = DAC0_RST        PB5  = EEPROM_WP
 * PE3  = SAI1_SD_B (AF6)   PE11 = SAI2_SD_B(AF10) PB6  = I2C3_SCL
 * PE4  = SAI1_FS_A (AF6)   PE12 = free            PB7  = I2C3_SDA
 * PE5  = SAI1_SCK_A (AF6)  PE13 = free            PB8  = I2C1_SCL
 * PE6  = SAI1_SD_A (AF6)   PE14 = TP2             PB9  = I2C1_SDA
 * PE7  = DAC0_AMUTEI       PE15 = free            PB11 = free
 *
 * PD8  = SAI3_SCK_B (AF6)  PD14 = ADC_MODE0       PB13 = DAC3_AMUTEO
 * PD9  = SAI3_SD_B (AF6)   PD15 = ADC_MODE1       PB14 = DAC3_AMUTEI
 * PD10 = SAI3_FS_B (AF6)                          PB15 = DAC3_RST
 * PD11 = SAI2_SD_A (AF10)  PA0  = UART4_TX
 * PD12 = SAI2_FS_A (AF10)  PA1  = UART4_RX        PC0  = free
 * PD13 = SAI2_SCK_A(AF10)  PA4  = ADC_FSYNC       PC1  = free
 *                          PA5  = ADC_OSR          PC4  = LED1
 * PA6  = ADC_BYPASS        PA10 = PWR_EN_3V3A     PC5  = LED2
 * PA7  = ADC_PDWN          PA11 = USB_DM          PC6  = ADC_FMT1
 * PA8  = TP3               PA12 = USB_DP          PC7  = ADC_FMT0
 *                          PA13 = SWDIO           PC8  = DAC1_RST
 *                          PA14 = SWCLK           PC10 = DAC2_RST
 *                                                 PC11 = DAC2_AMUTEI
 *                                                 PC12 = DAC2_AMUTEO
 *
 * PB12 = EPD_CS            SPI2: PB13=SCK, PB15=MOSI (shared w/ DAC3!)
 * EPD: PD4=DC, PD5=RST, PD6=BUSY
 * ============================================================================ */

#endif /* __PIN_DEFINITIONS_H */
