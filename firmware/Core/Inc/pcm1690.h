/**
 * @file pcm1690.h
 * @brief PCM1690 8-channel DAC driver (TDM8 mode)
 *
 * Texas Instruments PCM1690: 8-ch, 24-bit, 192kHz DAC
 * Interface: I2C control + TDM8 audio data via SAI
 *
 * DS9 board has 4x PCM1690 (32 channels total):
 *   DAC0 (0x94) → SAI1_A channels 0-7
 *   DAC1 (0x96) → SAI1_B channels 8-15
 *   DAC2 (0x98) → SAI2_A channels 16-23
 *   DAC3 (0x9A) → SAI2_B channels 24-31
 */

#ifndef __PCM1690_H
#define __PCM1690_H

#include "stm32h7xx_hal.h"

/* ============================================================================
 * I2C Addresses (8-bit, includes R/W bit - use directly with HAL)
 * ============================================================================ */
/* Base address 0x4C (7-bit). ADR1=pin23, ADR0=pin22 of PCM1690.
 * 8-bit write address = (7-bit << 1) */
#define PCM1690_ADDR_DAC0       0x98    // U3: ADR1=0,ADR0=0 → 7-bit 0x4C
#define PCM1690_ADDR_DAC1       0x9C    // U4: ADR1=1,ADR0=0 → 7-bit 0x4E
#define PCM1690_ADDR_DAC2       0x9A    // U2: ADR1=0,ADR0=1 → 7-bit 0x4D
#define PCM1690_ADDR_DAC3       0x9E    // U8: ADR1=1,ADR0=1 → 7-bit 0x4F

/* ============================================================================
 * Register Addresses
 * ============================================================================ */
#define PCM1690_REG_SYSTEM      64      // 0x40 - System control
#define PCM1690_REG_FORMAT      65      // 0x41 - Audio format
#define PCM1690_REG_OPERATION   66      // 0x42 - DAC operation control
#define PCM1690_REG_MUTE        68      // 0x44 - Soft mute control
#define PCM1690_REG_DEEMPH      70      // 0x46 - De-emphasis & flags
#define PCM1690_REG_ATTEN_CH1   72      // 0x48 - Channel 1 attenuation
#define PCM1690_REG_ATTEN_CH8   79      // 0x4F - Channel 8 attenuation

/* ============================================================================
 * Register 64 - System Control
 * ============================================================================ */
#define PCM1690_SRDA_AUTO       0x00    // Auto-detect sample rate
#define PCM1690_SRDA_SINGLE     0x01    // Single rate (fs < 50kHz)
#define PCM1690_SRDA_DUAL       0x02    // Dual rate (fs < 100kHz)
#define PCM1690_SRDA_QUAD       0x03    // Quad rate (fs < 200kHz)
#define PCM1690_SYSTEM_RESET    0x40    // Trigger system reset (auto-clear)
#define PCM1690_MODE_RESET      0x80    // Trigger mode reset (auto-clear)

/* ============================================================================
 * Register 65 - Audio Format (FMTDA[3:0] in bits 3:0)
 * ============================================================================ */
#define PCM1690_FMT_I2S         0x00    // Standard I2S (2-ch)
#define PCM1690_FMT_LJ          0x01    // Left-justified (2-ch)
#define PCM1690_FMT_RJ24        0x02    // Right-justified 24-bit
#define PCM1690_FMT_RJ16        0x03    // Right-justified 16-bit
#define PCM1690_FMT_DSP_I2S     0x04    // DSP I2S mode
#define PCM1690_FMT_DSP_LJ      0x05    // DSP Left-justified
#define PCM1690_FMT_TDM_I2S     0x06    // TDM I2S mode (8-ch) ← DS9 config
#define PCM1690_FMT_TDM_LJ      0x07    // TDM Left-justified (8-ch)

/* ============================================================================
 * Register 66 - Operation Control
 * Bits 7:4 = OPEDA[3:0] (0=enabled, 1=disabled per pair)
 * Bits 3:0 = FLT[3:0] (0=sharp, 1=slow roll-off per pair)
 * ============================================================================ */
#define PCM1690_ALL_DAC_ON      0x00    // All DACs enabled, sharp roll-off
#define PCM1690_ALL_DAC_OFF     0xF0    // All DACs disabled

/* ============================================================================
 * Register 68 - Soft Mute Control (per channel, bit=1 → muted)
 * ============================================================================ */
#define PCM1690_MUTE_NONE       0x00    // All channels unmuted
#define PCM1690_MUTE_ALL        0xFF    // All channels muted

/* ============================================================================
 * Attenuation Values (Registers 72-79)
 * Fine mode (DAMS=0): 0.5 dB steps, 0-63 dB range
 * Formula: ATDAx = 255 - (atten_dB / 0.5)
 * ============================================================================ */
#define PCM1690_ATTEN_0DB       0xFF    // No attenuation
#define PCM1690_ATTEN_6DB       0xF3    // -6 dB
#define PCM1690_ATTEN_12DB      0xE7    // -12 dB
#define PCM1690_ATTEN_20DB      0xD7    // -20 dB
#define PCM1690_ATTEN_40DB      0xAF    // -40 dB
#define PCM1690_ATTEN_MUTE      0x80    // Mute (infinite attenuation)

/* ============================================================================
 * Status codes
 * ============================================================================ */
typedef enum {
    PCM1690_OK = 0,
    PCM1690_ERR_I2C,
    PCM1690_ERR_NOT_FOUND,
    PCM1690_ERR_VERIFY
} PCM1690_Status;

/* ============================================================================
 * Hardware pin configuration (per DAC instance)
 * ============================================================================ */
typedef struct {
    GPIO_TypeDef *rst_port;       // RST pin port (active LOW)
    uint16_t      rst_pin;        // RST pin number
    GPIO_TypeDef *amutei_port;    // AMUTEI pin port (LOW=muted, HIGH=unmuted)
    uint16_t      amutei_pin;     // AMUTEI pin number
} PCM1690_Pins;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Full power-up sequence for one PCM1690
 * @param hi2c I2C handle (e.g. &hi2c1)
 * @param dev_addr 8-bit I2C address (0x94, 0x96, 0x98, 0x9A)
 * @param pins Hardware pin config (RST, AMUTEI). NULL to skip GPIO control.
 * @return PCM1690_OK on success
 *
 * Sequence: RST LOW → wait 1ms → RST HIGH → wait 10ms →
 *           I2C ready check → ConfigTDM → Verify → Unmute
 *
 * IMPORTANT: MCLK (SAI) must be running BEFORE calling this function.
 */
PCM1690_Status PCM1690_Init(I2C_HandleTypeDef *hi2c, uint8_t dev_addr,
                             const PCM1690_Pins *pins);

/**
 * @brief Configure PCM1690 for TDM8 I2S mode
 * @param hi2c I2C handle (e.g. &hi2c1)
 * @param dev_addr 8-bit I2C address (0x94, 0x96, 0x98, 0x9A)
 * @return PCM1690_OK on success
 *
 * Configures: auto sample rate, TDM I2S format, all DACs enabled,
 *             de-emphasis off, all channels 0dB
 */
PCM1690_Status PCM1690_ConfigTDM(I2C_HandleTypeDef *hi2c, uint8_t dev_addr);

/**
 * @brief Check if PCM1690 responds on I2C bus
 * @param hi2c I2C handle
 * @param dev_addr 8-bit I2C address
 * @return 1 if device responds, 0 otherwise
 */
uint8_t PCM1690_IsReady(I2C_HandleTypeDef *hi2c, uint8_t dev_addr);

/**
 * @brief Set attenuation for a single channel
 * @param hi2c I2C handle
 * @param dev_addr 8-bit I2C address
 * @param channel Channel number (1-8)
 * @param atten Attenuation value (0xFF=0dB, 0x80=mute)
 * @return PCM1690_OK on success
 */
PCM1690_Status PCM1690_SetAttenuation(I2C_HandleTypeDef *hi2c, uint8_t dev_addr,
                                       uint8_t channel, uint8_t atten);

/**
 * @brief Soft mute/unmute channels
 * @param hi2c I2C handle
 * @param dev_addr 8-bit I2C address
 * @param mute_mask Bitmask (bit0=ch1, ..., bit7=ch8, 1=muted)
 * @return PCM1690_OK on success
 */
PCM1690_Status PCM1690_SoftMute(I2C_HandleTypeDef *hi2c, uint8_t dev_addr,
                                 uint8_t mute_mask);

/**
 * @brief Read a register value (for verification)
 * @param hi2c I2C handle
 * @param dev_addr 8-bit I2C address
 * @param reg Register address
 * @param data Pointer to store read value
 * @return PCM1690_OK on success
 */
PCM1690_Status PCM1690_ReadReg(I2C_HandleTypeDef *hi2c, uint8_t dev_addr,
                                uint8_t reg, uint8_t *data);

/**
 * @brief Verify PCM1690 configuration matches expected values
 * @param hi2c I2C handle
 * @param dev_addr 8-bit I2C address
 * @return PCM1690_OK if config matches TDM I2S mode
 */
PCM1690_Status PCM1690_Verify(I2C_HandleTypeDef *hi2c, uint8_t dev_addr);

#endif /* __PCM1690_H */
