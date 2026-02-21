/**
 * @file    eeprom_config.c
 * @brief   EEPROM configuration persistence for DS9 beamforming
 *
 * CAT24C256 (U6): 32KB, I2C4, address 0xA0, page size 64 bytes.
 * Write cycle time: 5ms max per page write.
 *
 * NOTE: DS9 PCB routes EEPROM to PB6(SCL)/PB7(SDA).
 * On STM32H743, PB6/PB7 AF4 = I2C1 (already used for DACs on PB8/PB9).
 * PB6/PB7 AF6 = I2C4 — this is the correct peripheral for EEPROM.
 * pin_definitions.h labels it as "I2C3" but that's a schematic error.
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

/* CAT24C256 page size */
#define EEPROM_PAGE_SIZE    64
#define EEPROM_I2C_TIMEOUT  100  /* ms */

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
 * I2C4 Init (PB6=SCL, PB7=SDA, AF6)
 * ============================================================ */
static uint8_t eeprom_available = 0;

HAL_StatusTypeDef EEPROM_Init(void)
{
    /* Init I2C4 GPIO: PB6=SCL, PB7=SDA, AF6 (NOT AF4 which is I2C1) */
    {
        GPIO_InitTypeDef gpio = {0};
        __HAL_RCC_GPIOB_CLK_ENABLE();
        gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;  /* PB6 + PB7 */
        gpio.Mode = GPIO_MODE_AF_OD;
        gpio.Pull = GPIO_NOPULL;  /* External 5.1k pullups */
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF6_I2C4;  /* AF6 for I2C4 on PB6/PB7 */
        HAL_GPIO_Init(GPIOB, &gpio);
    }

    /* Enable I2C4 clock */
    __HAL_RCC_I2C4_CLK_ENABLE();

    memset(&hi2c4, 0, sizeof(hi2c4));
    hi2c4.Instance = I2C4;
    hi2c4.Init.Timing = 0x307075B1;  /* 100kHz SM @ 120MHz APB */
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

    /* Wait for write cycle to complete (5ms max for CAT24C256) */
    HAL_Delay(6);

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

    /* Read config data */
    if (eeprom_read(addr, (uint8_t*)config, config_size) != HAL_OK)
        return EEPROM_ERR_I2C;
    addr += config_size;

    /* Read and verify CRC */
    uint16_t stored_crc;
    if (eeprom_read(addr, (uint8_t*)&stored_crc, 2) != HAL_OK)
        return EEPROM_ERR_I2C;

    uint16_t calc_crc = crc16_ccitt((const uint8_t*)config, config_size);
    if (calc_crc != stored_crc)
        return EEPROM_ERR_CRC;

    return EEPROM_OK;
}
