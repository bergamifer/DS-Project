/**
 * @file epaper.h
 * @brief SSD1681 E-Paper Driver for WeAct 1.54" Display (200x200)
 * @note Uses SPI2 on STM32H743
 */

#ifndef __EPAPER_H
#define __EPAPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <string.h>

/* Display dimensions */
#define EPD_WIDTH       200
#define EPD_HEIGHT      200
#define EPD_BUFFER_SIZE ((EPD_WIDTH * EPD_HEIGHT) / 8)  /* 5000 bytes */

/* Colors */
#define EPD_WHITE       0xFF
#define EPD_BLACK       0x00

/* SSD1681 Commands */
#define SSD1681_SW_RESET            0x12
#define SSD1681_DRIVER_OUTPUT       0x01
#define SSD1681_DATA_ENTRY_MODE     0x11
#define SSD1681_SET_RAM_X_ADDR      0x44
#define SSD1681_SET_RAM_Y_ADDR      0x45
#define SSD1681_SET_RAM_X_COUNT     0x4E
#define SSD1681_SET_RAM_Y_COUNT     0x4F
#define SSD1681_BORDER_WAVEFORM     0x3C
#define SSD1681_TEMP_SENSOR_CTRL    0x18
#define SSD1681_DISPLAY_UPDATE_1    0x21
#define SSD1681_DISPLAY_UPDATE_2    0x22
#define SSD1681_MASTER_ACTIVATION   0x20
#define SSD1681_WRITE_RAM_BW        0x24
#define SSD1681_WRITE_RAM_RED       0x26
#define SSD1681_DEEP_SLEEP          0x10

/* Pin definitions - adjust to your wiring */
/* SPI2: PB13=SCK, PB15=MOSI */
#define EPD_CS_PIN      GPIO_PIN_12
#define EPD_CS_PORT     GPIOB
#define EPD_DC_PIN      GPIO_PIN_4
#define EPD_DC_PORT     GPIOD
#define EPD_RST_PIN     GPIO_PIN_5
#define EPD_RST_PORT    GPIOD
#define EPD_BUSY_PIN    GPIO_PIN_6
#define EPD_BUSY_PORT   GPIOD

/* Pin macros */
#define EPD_CS_LOW()    HAL_GPIO_WritePin(EPD_CS_PORT, EPD_CS_PIN, GPIO_PIN_RESET)
#define EPD_CS_HIGH()   HAL_GPIO_WritePin(EPD_CS_PORT, EPD_CS_PIN, GPIO_PIN_SET)
#define EPD_DC_LOW()    HAL_GPIO_WritePin(EPD_DC_PORT, EPD_DC_PIN, GPIO_PIN_RESET)
#define EPD_DC_HIGH()   HAL_GPIO_WritePin(EPD_DC_PORT, EPD_DC_PIN, GPIO_PIN_SET)
#define EPD_RST_LOW()   HAL_GPIO_WritePin(EPD_RST_PORT, EPD_RST_PIN, GPIO_PIN_RESET)
#define EPD_RST_HIGH()  HAL_GPIO_WritePin(EPD_RST_PORT, EPD_RST_PIN, GPIO_PIN_SET)
#define EPD_BUSY_READ() HAL_GPIO_ReadPin(EPD_BUSY_PORT, EPD_BUSY_PIN)

/* Font sizes */
#define EPD_FONT_8      8
#define EPD_FONT_12     12
#define EPD_FONT_16     16
#define EPD_FONT_24     24

/* Status codes */
typedef enum {
    EPD_OK = 0,
    EPD_ERROR,
    EPD_BUSY_TIMEOUT
} EPD_Status;

/* Function prototypes */

/* Initialization and control */
EPD_Status EPD_Init(SPI_HandleTypeDef *hspi);
void EPD_Reset(void);
EPD_Status EPD_WaitUntilIdle(uint32_t timeout_ms);
void EPD_DeepSleep(void);

/* Display update */
void EPD_Update(void);
void EPD_UpdatePartial(void);
void EPD_Display(uint8_t *image);
void EPD_DisplayPartial(uint8_t *image, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void EPD_Clear(uint8_t color);

/* Drawing functions (operate on buffer) */
void EPD_SetPixel(uint8_t *buffer, uint16_t x, uint16_t y, uint8_t color);
void EPD_DrawLine(uint8_t *buffer, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t color);
void EPD_DrawRect(uint8_t *buffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color);
void EPD_FillRect(uint8_t *buffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color);
void EPD_DrawChar(uint8_t *buffer, uint16_t x, uint16_t y, char c, uint8_t font_size, uint8_t color);
void EPD_DrawString(uint8_t *buffer, uint16_t x, uint16_t y, const char *str, uint8_t font_size, uint8_t color);
void EPD_DrawNumber(uint8_t *buffer, uint16_t x, uint16_t y, int32_t num, uint8_t font_size, uint8_t color);

/* Buffer operations */
void EPD_FillBuffer(uint8_t *buffer, uint8_t color);

#ifdef __cplusplus
}
#endif

#endif /* __EPAPER_H */
