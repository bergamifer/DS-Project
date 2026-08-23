/**
 * @file    eeprom_config.c
 * @brief   EEPROM configuration persistence for DS10 beamforming
 *
 * CAT24C256 (U8 on DS10): 32KB, I2C4, address 0xA0, page size 64 bytes.
 * Write cycle time: 5ms max per page write.
 *
 * I2C4 pins: PB6(SCL) / PB7(SDA) with AF6 (confirmado en bring-up: la EEPROM
 * responde en 0x50 con AF6; el AF11 del design review era un error).
 * GPIO config uses EEPROM_I2C_AF macro from pin_definitions.h.
 *
 * EEPROM layout at base address:
 *   [0x00] uint32_t magic    = 0xD509BEEF
 *   [0x04] uint16_t version  = 1
 *   [0x06] uint8_t  config_data[config_size]
 *   [0x06+N] uint16_t crc16
 */

#include "eeprom_config.h"
#include "pin_definitions.h"
#include <string.h>

/* Default no-op; main.c la redefine para servir los fills de audio. */
__weak void EEPROM_Yield(void) { }

/* CAT24C256 page size */
#define EEPROM_PAGE_SIZE    64
/* Máximo de bytes por transacción I2C. El chip acepta los 64 de una página,
 * pero HAL_I2C_Mem_Write BLOQUEA mientras transfiere y a 100 kHz una página
 * entera son (1 dir + 2 mem + 64 datos) × 9 bits ≈ **6 ms** — por encima del
 * deadline de 5 ms del fill de audio. Resultado: exactamente un medio buffer
 * repetido en los 32 canales por página, o sea el pop que quedaba al guardar.
 *
 * Con 32 bytes la transferencia baja a ~3,2 ms y entra con margen. El costo son
 * más ciclos de escritura del chip, pero esos SÍ están yieldeados (el bucle de
 * espera de abajo sirve los fills), así que no se oyen. Escribir de a media
 * página es legal: el CAT24C256 sólo prohíbe cruzar el límite de página. */
#define EEPROM_WRITE_CHUNK  32

#define EEPROM_I2C_TIMEOUT  100  /* ms */

/* Buffer de verificación: la config se lee acá, se le chequea el CRC y sólo
 * entonces se copia al destino. 256 basta de sobra para beam_config_t (108 B) y
 * deja margen; si algún día no alcanza, LoadConfig devuelve EEPROM_ERR_SIZE en
 * vez de desbordar. */
static uint8_t cfg_scratch[EEPROM_CFG_MAX_SIZE];

/* I2C4 handle (local to this module) */
static I2C_HandleTypeDef hi2c4;

static inline void wp_disable(void)
{
    HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN, GPIO_PIN_RESET);
}

static inline void wp_enable(void)
{
    HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN, GPIO_PIN_SET);
}

/* ============================================================
 * CRC-16/CCITT (polynomial 0x1021, init 0xFFFF)
 * ============================================================ */
static uint16_t crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ============================================================
 * I2C4 Init (PB6=SCL, PB7=SDA, AF11 on STM32H743 LQFP100)
 * ============================================================ */
static uint8_t eeprom_available = 0;

HAL_StatusTypeDef EEPROM_Init(void)
{
    /* I2C4 GPIO, clock and NVIC are configured by HAL_I2C_MspInit (stm32h7xx_hal_msp.c).
     * Just configure the I2C handle and call HAL_I2C_Init — MspInit runs automatically. */
    memset(&hi2c4, 0, sizeof(hi2c4));
    hi2c4.Instance = I2C4;
    hi2c4.Init.Timing = 0x307075B1;  /* 100kHz SM @ 120MHz D3PCLK1 */
    hi2c4.Init.OwnAddress1 = 0;
    hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c4.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c4.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c4.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    HAL_StatusTypeDef status = HAL_I2C_Init(&hi2c4);
    if (status != HAL_OK) {
        eeprom_available = 0;
        return status;
    }

    HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE);
    HAL_I2CEx_ConfigDigitalFilter(&hi2c4, 0);

    /* Check if EEPROM responds */
    if (HAL_I2C_IsDeviceReady(&hi2c4, EEPROM_ADDR, 3, EEPROM_I2C_TIMEOUT) == HAL_OK) {
        eeprom_available = 1;
    } else {
        eeprom_available = 0;
    }

    return HAL_OK;
}

uint8_t EEPROM_IsAvailable(void)
{
    return eeprom_available;
}

/* ============================================================
 * Low-level EEPROM read/write with 16-bit addressing
 * CAT24C256 uses 2-byte memory address (big-endian)
 * ============================================================ */

static HAL_StatusTypeDef eeprom_write_page(uint16_t mem_addr, const uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status;

    wp_disable();

    status = HAL_I2C_Mem_Write(&hi2c4, EEPROM_ADDR, mem_addr,
                                I2C_MEMADD_SIZE_16BIT, (uint8_t*)data, len,
                                EEPROM_I2C_TIMEOUT);

    wp_enable();

    /* Wait for write cycle to complete (5ms max for CAT24C256).
     *
     * NO usar HAL_Delay acá: el guardado son 2 páginas = ~12 ms de bloqueo, más
     * que el deadline de 10 ms del fill de audio, y cada deadline perdido es un
     * medio buffer repetido en los 32 canales = un "pop" audible. Era el pop más
     * fuerte de los tres (se oía al cambiar de modo, 3 s después por el
     * debounce). EEPROM_Yield() deja que el main loop sirva los fills mientras
     * el chip termina su ciclo de escritura. */
    if (status == HAL_OK) {
        uint32_t until = HAL_GetTick() + 6;
        while ((int32_t)(HAL_GetTick() - until) < 0) EEPROM_Yield();
    }

    return status;
}

static HAL_StatusTypeDef eeprom_write(uint16_t mem_addr, const uint8_t *data, uint16_t len)
{
    /* Write in page-aligned chunks to respect CAT24C256 page boundaries */
    uint16_t offset = 0;
    while (offset < len) {
        /* Bytes remaining in current page */
        uint16_t page_offset = (mem_addr + offset) % EEPROM_PAGE_SIZE;
        uint16_t page_remain = EEPROM_PAGE_SIZE - page_offset;
        uint16_t chunk = len - offset;
        if (chunk > page_remain) chunk = page_remain;
        if (chunk > EEPROM_WRITE_CHUNK) chunk = EEPROM_WRITE_CHUNK;

        HAL_StatusTypeDef status = eeprom_write_page(mem_addr + offset, &data[offset], chunk);
        if (status != HAL_OK) return status;

        offset += chunk;
    }
    return HAL_OK;
}

static HAL_StatusTypeDef eeprom_read(uint16_t mem_addr, uint8_t *data, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c4, EEPROM_ADDR, mem_addr,
                             I2C_MEMADD_SIZE_16BIT, data, len,
                             EEPROM_I2C_TIMEOUT);
}

/* ============================================================
 * Public API
 * ============================================================ */

eeprom_status_t EEPROM_Test(void)
{
    if (HAL_I2C_IsDeviceReady(&hi2c4, EEPROM_ADDR, 3, EEPROM_I2C_TIMEOUT) != HAL_OK)
        return EEPROM_ERR_I2C;
    return EEPROM_OK;
}

eeprom_status_t EEPROM_SaveConfig(const void *config, uint16_t config_size)
{
    uint16_t addr = EEPROM_CFG_BASE_ADDR;

    /* Write magic */
    uint32_t magic = EEPROM_CFG_MAGIC;
    if (eeprom_write(addr, (uint8_t*)&magic, 4) != HAL_OK)
        return EEPROM_ERR_I2C;
    addr += 4;

    /* Write version */
    uint16_t version = EEPROM_CFG_VERSION;
    if (eeprom_write(addr, (uint8_t*)&version, 2) != HAL_OK)
        return EEPROM_ERR_I2C;
    addr += 2;

    /* Write config data */
    if (eeprom_write(addr, (const uint8_t*)config, config_size) != HAL_OK)
        return EEPROM_ERR_I2C;
    addr += config_size;

    /* Write CRC16 over config data */
    uint16_t crc = crc16_ccitt((const uint8_t*)config, config_size);
    if (eeprom_write(addr, (uint8_t*)&crc, 2) != HAL_OK)
        return EEPROM_ERR_I2C;

    return EEPROM_OK;
}

eeprom_status_t EEPROM_LoadConfig(void *config, uint16_t config_size)
{
    uint16_t addr = EEPROM_CFG_BASE_ADDR;

    if (config_size > sizeof(cfg_scratch)) return EEPROM_ERR_SIZE;

    /* Read and verify magic */
    uint32_t magic;
    if (eeprom_read(addr, (uint8_t*)&magic, 4) != HAL_OK)
        return EEPROM_ERR_I2C;
    if (magic != EEPROM_CFG_MAGIC)
        return EEPROM_ERR_MAGIC;
    addr += 4;

    /* Read and verify version */
    uint16_t version;
    if (eeprom_read(addr, (uint8_t*)&version, 2) != HAL_OK)
        return EEPROM_ERR_I2C;
    if (version != EEPROM_CFG_VERSION)
        return EEPROM_ERR_VERSION;
    addr += 2;

    /* Los datos van a un buffer intermedio, NO al destino: recién se copian si
     * el CRC valida. Leer directo sobre `config` dejaba al llamador corriendo
     * con datos corruptos aunque detectara el error — y "los defaults" que creía
     * conservar ya habían sido pisados. */
    if (eeprom_read(addr, cfg_scratch, config_size) != HAL_OK)
        return EEPROM_ERR_I2C;
    addr += config_size;

    /* Read and verify CRC */
    uint16_t stored_crc;
    if (eeprom_read(addr, (uint8_t*)&stored_crc, 2) != HAL_OK)
        return EEPROM_ERR_I2C;

    uint16_t calc_crc = crc16_ccitt(cfg_scratch, config_size);
    if (calc_crc != stored_crc)
        return EEPROM_ERR_CRC;

    memcpy(config, cfg_scratch, config_size);
    return EEPROM_OK;
}
