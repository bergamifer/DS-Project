/**
 * @file pin_definitions.h
 * @brief Pin definitions for Domosonica DS9 v2.0 - STM32H743VIT6
 *
 * Hardware: STM32H743VIT6 LQFP100 (soldado directo al PCB)
 * Crystal: 25 MHz HSE (Y2), 32.768 kHz LSE (Y1)
 * Version: 2.0 - Actualizado desde DS.d356 (Enero 2026)
 *
 * Changes from v1.0:
 *   - 4x DACs (32 channels) instead of 2x (16 channels)
 *   - SAI2 added for DAC2/DAC3
 *   - I2C3 added for EEPROM
 *   - Extended ADC control (8 GPIO pins)
 *   - UART4 for ESP32 communication (was UART7)
 *   - 4 status LEDs (was 1)
 */

#ifndef __PIN_DEFINITIONS_H
#define __PIN_DEFINITIONS_H

/* ============================================================================
 * SAI1 - Audio Output (DAC0, DAC1) - Channels 0-15
 * ============================================================================ */

// SAI1 Master Clock (shared by all audio chips via R21 33 Ohm)
#define SAI1_MCLK_PIN           GPIO_PIN_2
#define SAI1_MCLK_PORT          GPIOE
#define SAI1_MCLK_AF            GPIO_AF6_SAI1

// SAI1_A (DAC0 - U3 PCM1690) - Channels 0-7
#define SAI1_FS_A_PIN           GPIO_PIN_7      // PE7 - Frame Sync
#define SAI1_FS_A_PORT          GPIOE
#define SAI1_SCK_A_PIN          GPIO_PIN_8      // PE8 - Bit Clock
#define SAI1_SCK_A_PORT         GPIOE
#define SAI1_SD_A_PIN           GPIO_PIN_9      // PE9 - Serial Data
#define SAI1_SD_A_PORT          GPIOE

// SAI1_B (DAC1 - U4 PCM1690) - Channels 8-15
#define SAI1_FS_B_PIN           GPIO_PIN_4      // PE4 - Frame Sync
#define SAI1_FS_B_PORT          GPIOE
#define SAI1_SCK_B_PIN          GPIO_PIN_5      // PE5 - Bit Clock
#define SAI1_SCK_B_PORT         GPIOE
#define SAI1_SD_B_PIN           GPIO_PIN_6      // PE6 - Serial Data
#define SAI1_SD_B_PORT          GPIOE

/* ============================================================================
 * SAI2 - Audio Output (DAC2, DAC3) - Channels 16-31
 * ============================================================================ */

// SAI2_A (DAC2 - U2 PCM1690) - Channels 16-23
#define SAI2_FS_A_PIN           GPIO_PIN_13     // PD13 - Frame Sync
#define SAI2_FS_A_PORT          GPIOD
#define SAI2_SCK_A_PIN          GPIO_PIN_12     // PD12 - Bit Clock
#define SAI2_SCK_A_PORT         GPIOD
#define SAI2_SD_A_PIN           GPIO_PIN_11     // PD11 - Serial Data
#define SAI2_SD_A_PORT          GPIOD
#define SAI2_A_AF               GPIO_AF10_SAI2

// SAI2_B (DAC3 - U8 PCM1690) - Channels 24-31
#define SAI2_FS_B_PIN           GPIO_PIN_10     // PD10 - Frame Sync
#define SAI2_FS_B_PORT          GPIOD
#define SAI2_SCK_B_PIN          GPIO_PIN_9      // PD9 - Bit Clock
#define SAI2_SCK_B_PORT         GPIOD
#define SAI2_SD_B_PIN           GPIO_PIN_8      // PD8 - Serial Data
#define SAI2_SD_B_PORT          GPIOD
#define SAI2_B_AF               GPIO_AF10_SAI2

/* ============================================================================
 * SAI4 - Audio Input (ADC - U1 PCM1802) - 2 Channels
 * ============================================================================ */

#define SAI4_FS_B_PIN           GPIO_PIN_0      // PC0 - Frame Sync (LRCK)
#define SAI4_FS_B_PORT          GPIOC
#define SAI4_SCK_B_PIN          GPIO_PIN_1      // PC1 - Bit Clock (BCK)
#define SAI4_SCK_B_PORT         GPIOC
#define SAI4_SD_B_PIN           GPIO_PIN_11     // PB11 - Serial Data (DOUT)
#define SAI4_SD_B_PORT          GPIOB
#define SAI4_B_AF               GPIO_AF10_SAI4

/* ============================================================================
 * I2C1 - DAC Configuration Bus (4x PCM1690)
 * Pull-ups: 5.1kΩ (optimal: 4.7kΩ for 400kHz)
 * ============================================================================ */

#define I2C1_SCL_PIN            GPIO_PIN_8      // PB8
#define I2C1_SCL_PORT           GPIOB
#define I2C1_SDA_PIN            GPIO_PIN_9      // PB9
#define I2C1_SDA_PORT           GPIOB
#define I2C1_AF                 GPIO_AF4_I2C1

// PCM1690 I2C Addresses (8-bit write addresses from datasheet)
// Note: HAL uses 8-bit addresses with R/W bit, these are write addresses
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
 * Pull-ups: 5.1kΩ (optimal: 4.7kΩ for 400kHz)
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
 * ============================================================================ */

#define DAC0_RST_PIN            GPIO_PIN_10     // PE10 - U3 Reset
#define DAC0_RST_PORT           GPIOE
#define DAC1_RST_PIN            GPIO_PIN_3      // PE3 - U4 Reset
#define DAC1_RST_PORT           GPIOE
#define DAC2_RST_PIN            GPIO_PIN_10     // PC10 - U2 Reset
#define DAC2_RST_PORT           GPIOC
#define DAC3_RST_PIN            GPIO_PIN_15     // PB15 - U8 Reset
#define DAC3_RST_PORT           GPIOB

/* ============================================================================
 * DAC Control Pins - Mute Input/Output
 * AMUTEI: Active LOW (LOW = muted, HIGH = unmuted)
 * AMUTEO: Status output from DAC
 * ============================================================================ */

// DAC0 (U3) Mute
#define DAC0_AMUTEI_PIN         GPIO_PIN_11     // PE11 - Mute Input
#define DAC0_AMUTEI_PORT        GPIOE
#define DAC0_AMUTEO_PIN         GPIO_PIN_12     // PE12 - Mute Output (status)
#define DAC0_AMUTEO_PORT        GPIOE

// DAC1 (U4) Mute
#define DAC1_AMUTEI_PIN         GPIO_PIN_1      // PE1 - Mute Input
#define DAC1_AMUTEI_PORT        GPIOE
#define DAC1_AMUTEO_PIN         GPIO_PIN_0      // PE0 - Mute Output (status)
#define DAC1_AMUTEO_PORT        GPIOE

// DAC2 (U2) Mute
#define DAC2_AMUTEI_PIN         GPIO_PIN_11     // PC11 - Mute Input
#define DAC2_AMUTEI_PORT        GPIOC
#define DAC2_AMUTEO_PIN         GPIO_PIN_12     // PC12 - Mute Output (status)
#define DAC2_AMUTEO_PORT        GPIOC

// DAC3 (U8) Mute
#define DAC3_AMUTEI_PIN         GPIO_PIN_14     // PB14 - Mute Input
#define DAC3_AMUTEI_PORT        GPIOB
#define DAC3_AMUTEO_PIN         GPIO_PIN_13     // PB13 - Mute Output (status)
#define DAC3_AMUTEO_PORT        GPIOB

/* ============================================================================
 * ADC Control Pins (PCM1802 - U1) - Extended in v2.0
 * 8 GPIO pins for full control
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
 * Note: DS9 v1 fabrication bug - all LEDs are red (C84256)
 * ============================================================================ */

#define LED1_PIN                GPIO_PIN_4      // PC4 - D6 (Yellow design, Red fab)
#define LED1_PORT               GPIOC
#define LED2_PIN                GPIO_PIN_5      // PC5 - D7 (Red - correct)
#define LED2_PORT               GPIOC
#define LED3_PIN                GPIO_PIN_0      // PB0 - D11 (Green design, Red fab)
#define LED3_PORT               GPIOB
#define LED4_PIN                GPIO_PIN_1      // PB1 - D10 (White design, Red fab)
#define LED4_PORT               GPIOB

// Suggested LED functions
#define LED_POWER_PIN           LED1_PIN        // Power/Ready indicator
#define LED_POWER_PORT          LED1_PORT
#define LED_AUDIO_PIN           LED2_PIN        // Audio activity / Error
#define LED_AUDIO_PORT          LED2_PORT
#define LED_COMM_PIN            LED3_PIN        // Communication OK
#define LED_COMM_PORT           LED3_PORT
#define LED_STATUS_PIN          LED4_PIN        // Status / Heartbeat
#define LED_STATUS_PORT         LED4_PORT

/* ============================================================================
 * Power Control
 * ============================================================================ */

#define PWR_EN_3V3A_PIN         GPIO_PIN_10     // PA10 - Enable 3.3V analog
#define PWR_EN_3V3A_PORT        GPIOA

/* ============================================================================
 * Test Points
 * ============================================================================ */

#define TP1_PIN                 GPIO_PIN_13     // PE13 - General debug
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
 * MCLK = 12.288 MHz (256 × 48kHz)
 * Generated from PLL3 via SAI1_MCLK (PE2)
 * ============================================================================ */

#define AUDIO_SAMPLE_RATE       48000
#define AUDIO_MCLK_FREQ         12288000    // 256 × fs
#define AUDIO_BCK_FREQ          3072000     // 64 × fs (TDM8: 8 slots × 32 bits)
#define AUDIO_FS_FREQ           48000       // Sample rate

/* ============================================================================
 * Channel Configuration
 * ============================================================================ */

#define AUDIO_DAC_COUNT         4           // Number of PCM1690 chips
#define AUDIO_CHANNELS_PER_DAC  8           // Channels per PCM1690
#define AUDIO_TOTAL_CHANNELS    32          // Total output channels
#define AUDIO_ADC_CHANNELS      2           // Input channels (PCM1802)

#endif /* __PIN_DEFINITIONS_H */
