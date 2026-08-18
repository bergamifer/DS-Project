/**
 * @file    eeprom_config.h
 * @brief   EEPROM configuration persistence for DS10 beamforming
 *
 * Stores/loads beam_config_t to/from CAT24C256 EEPROM via I2C4 (PB6/PB7 AF11).
 * Layout: Magic(4) + Version(2) + beam_config_t + CRC16(2)
 */

#ifndef EEPROM_CONFIG_H
#define EEPROM_CONFIG_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

#define EEPROM_CFG_MAGIC       0xD509BEEF
#define EEPROM_CFG_VERSION     4   /* v2: += master_vol_db; v3: += last_mode;
                                    * v4: += in_gain_db (ganancia de entrada).
                                    * Un bump descarta lo guardado y vuelve a
                                    * los defaults — es una vez y a propósito:
                                    * leer bytes viejos con layout nuevo daría
                                    * un volumen o un modo arbitrarios. */
#define EEPROM_CFG_BASE_ADDR   0x0000   /* Start of EEPROM */

/* Result codes */
typedef enum {
    EEPROM_OK = 0,
    EEPROM_ERR_I2C,         /* I2C communication error */
    EEPROM_ERR_MAGIC,       /* No valid config found (first boot) */
    EEPROM_ERR_VERSION,     /* Version mismatch */
    EEPROM_ERR_CRC,         /* CRC mismatch (data corruption) */
} eeprom_status_t;

/**
 * @brief Initialize I2C3 peripheral + detect EEPROM
 * @return HAL_OK if I2C3 init succeeded (EEPROM may or may not be present)
 */
/**
 * @brief Hook llamado mientras la EEPROM completa su ciclo de escritura.
 *
 * Por defecto no hace nada (definición __weak). main.c la redefine para servir
 * los fills de audio pendientes: sin eso, los ~12 ms que bloquea un guardado
 * se comen deadlines de fill y producen un "pop" en los 32 canales.
 */
void EEPROM_Yield(void);

HAL_StatusTypeDef EEPROM_Init(void);

/**
 * @brief Check if EEPROM was detected during init
 * @return 1 if available, 0 otherwise
 */
uint8_t EEPROM_IsAvailable(void);

/**
 * @brief Save beamforming config to EEPROM
 * WARNING: Blocks ~12ms (page writes with HAL_Delay). Causes audio underruns.
 */
eeprom_status_t EEPROM_SaveConfig(const void *config, uint16_t config_size);

/**
 * @brief Load beamforming config from EEPROM
 */
eeprom_status_t EEPROM_LoadConfig(void *config, uint16_t config_size);

/**
 * @brief Test EEPROM connectivity (I2C IsDeviceReady)
 */
eeprom_status_t EEPROM_Test(void);

#endif /* EEPROM_CONFIG_H */
