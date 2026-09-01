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
#define EEPROM_CFG_VERSION     6   /* v2: += master_vol_db; v3: += last_mode;
                                    * v4: += in_gain_db (ganancia de entrada);
                                    * v5: += la calibración del AUTOFOCUS
                                    * (polaridades + foco medido), que antes se
                                    * perdía en cada reinicio.
                                    * v6: += autostart + autostart_fails — el
                                    * arranque autónomo de los amplificadores,
                                    * para que un nodo sin WiFi no quede mudo
                                    * (issue #19).
                                    * Un bump descarta lo guardado y vuelve a
                                    * los defaults — es una vez y a propósito:
                                    * leer bytes viejos con layout nuevo daría
                                    * un volumen o un modo arbitrarios. */
#define EEPROM_CFG_BASE_ADDR   0x0000   /* Start of EEPROM */
/* Techo del buffer de verificación (cfg_scratch). Una config más grande que
 * esto hace que EEPROM_SaveConfig devuelva EEPROM_ERR_SIZE — o sea que el
 * guardado deja de funcionar, y como es diferido el error aparecería como
 * "los ajustes no se recuerdan" mucho después de haber agregado el campo.
 * Está acá para poder chequearlo en COMPILACIÓN desde main.c. */
#define EEPROM_CFG_MAX_SIZE    256

/* Result codes */
typedef enum {
    EEPROM_OK = 0,
    EEPROM_ERR_I2C,         /* I2C communication error */
    EEPROM_ERR_MAGIC,       /* No valid config found (first boot) */
    EEPROM_ERR_VERSION,     /* Version mismatch */
    EEPROM_ERR_CRC,         /* CRC mismatch (data corruption) */
    EEPROM_ERR_SIZE,        /* config_size > el buffer de verificación */
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
 * @brief Save beamforming config to EEPROM.
 *
 * Tarda decenas de ms, pero NO produce underruns: las transferencias I2C van de
 * a 32 bytes (~3,2 ms, bajo el deadline de fill) y las esperas del ciclo de
 * escritura del chip llaman a EEPROM_Yield(). Llamar sólo desde el main loop.
 */
eeprom_status_t EEPROM_SaveConfig(const void *config, uint16_t config_size);

/**
 * @brief Igual que EEPROM_LoadConfig pero exigiendo una versión CONCRETA y un
 *        tamaño concreto, en vez de los actuales.
 *
 * Existe para migrar. Un bump de versión descartaba TODO lo guardado, y entre
 * eso está la calibración del AUTOFOCUS — que se persiste justamente porque
 * rehacerla en una instalación, con el micrófono en su lugar, es imposible.
 * Perderla cada vez que se agrega un campo a la config es un precio que no hay
 * ninguna razón para pagar: el struct crece SÓLO por el final, así que los bytes
 * viejos siguen significando lo mismo.
 *
 * El registro no guarda su tamaño, así que el llamador tiene que decir cuánto
 * medía esa versión. Ver `cfg_layouts[]` en main.c.
 */
eeprom_status_t EEPROM_LoadConfigAs(void *config, uint16_t config_size,
                                    uint16_t version);

/** @brief Qué versión hay guardada. EEPROM_ERR_MAGIC si no hay nada. */
eeprom_status_t EEPROM_PeekVersion(uint16_t *version);

/**
 * @brief Load beamforming config from EEPROM.
 *
 * ⚠️ `config` NO se toca salvo que TODO valide (magic + versión + CRC). Antes
 * se leía directo encima del destino y recién después se chequeaba el CRC: el
 * llamador imprimía "CRC error — using defaults" mientras corría con los datos
 * corruptos ya cargados. Con un byte dado vuelta eso arranca el instrumento con
 * volumen y ganancia de entrada arbitrarios sobre 32 amplificadores.
 */
eeprom_status_t EEPROM_LoadConfig(void *config, uint16_t config_size);

/**
 * @brief Test EEPROM connectivity (I2C IsDeviceReady)
 */
eeprom_status_t EEPROM_Test(void);

#endif /* EEPROM_CONFIG_H */
