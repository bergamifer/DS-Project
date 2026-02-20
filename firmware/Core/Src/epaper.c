/**
 * @file epaper.c
 * @brief SSD1681 E-Paper Driver Implementation for WeAct 1.54" (200x200)
 */

#include "epaper.h"
#include "epaper_fonts.h"

/* Private variables */
static SPI_HandleTypeDef *epd_spi = NULL;

/* Private function prototypes */
static void EPD_SendCommand(uint8_t cmd);
static void EPD_SendData(uint8_t data);
static void EPD_SendDataBurst(uint8_t *data, uint32_t len);
static void EPD_SetWindow(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end);
static void EPD_SetCursor(uint16_t x, uint16_t y);

/**
 * @brief Send command to display
 */
static void EPD_SendCommand(uint8_t cmd)
{
    EPD_DC_LOW();   /* Command mode */
    EPD_CS_LOW();
    HAL_SPI_Transmit(epd_spi, &cmd, 1, 100);
    EPD_CS_HIGH();
}

/**
 * @brief Send single data byte to display
 */
static void EPD_SendData(uint8_t data)
{
    EPD_DC_HIGH();  /* Data mode */
    EPD_CS_LOW();
    HAL_SPI_Transmit(epd_spi, &data, 1, 100);
    EPD_CS_HIGH();
}

/**
 * @brief Send burst of data to display
 */
static void EPD_SendDataBurst(uint8_t *data, uint32_t len)
{
    EPD_DC_HIGH();  /* Data mode */
    EPD_CS_LOW();
    HAL_SPI_Transmit(epd_spi, data, len, 1000);
    EPD_CS_HIGH();
}

/**
 * @brief Hardware reset the display
 */
void EPD_Reset(void)
{
    EPD_RST_HIGH();
    HAL_Delay(20);
    EPD_RST_LOW();
    HAL_Delay(2);
    EPD_RST_HIGH();
    HAL_Delay(20);
}

/**
 * @brief Wait until display is not busy
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return EPD_OK if ready, EPD_BUSY_TIMEOUT if timeout
 */
EPD_Status EPD_WaitUntilIdle(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (EPD_BUSY_READ() == GPIO_PIN_SET) {
        if ((HAL_GetTick() - start) > timeout_ms) {
            return EPD_BUSY_TIMEOUT;
        }
        HAL_Delay(10);
    }
    return EPD_OK;
}

/**
 * @brief Set RAM address window
 */
static void EPD_SetWindow(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)
{
    /* Set RAM X address */
    EPD_SendCommand(SSD1681_SET_RAM_X_ADDR);
    EPD_SendData((x_start >> 3) & 0x1F);
    EPD_SendData((x_end >> 3) & 0x1F);

    /* Set RAM Y address */
    EPD_SendCommand(SSD1681_SET_RAM_Y_ADDR);
    EPD_SendData(y_start & 0xFF);
    EPD_SendData((y_start >> 8) & 0x01);
    EPD_SendData(y_end & 0xFF);
    EPD_SendData((y_end >> 8) & 0x01);
}

/**
 * @brief Set RAM address cursor position
 */
static void EPD_SetCursor(uint16_t x, uint16_t y)
{
    /* Set RAM X counter */
    EPD_SendCommand(SSD1681_SET_RAM_X_COUNT);
    EPD_SendData((x >> 3) & 0x1F);

    /* Set RAM Y counter */
    EPD_SendCommand(SSD1681_SET_RAM_Y_COUNT);
    EPD_SendData(y & 0xFF);
    EPD_SendData((y >> 8) & 0x01);
}

/**
 * @brief Initialize the E-Paper display
 * @param hspi Pointer to SPI handle
 * @return EPD_OK on success, EPD_ERROR on failure
 */
EPD_Status EPD_Init(SPI_HandleTypeDef *hspi)
{
    if (hspi == NULL) {
        return EPD_ERROR;
    }

    epd_spi = hspi;

    /* Hardware reset */
    EPD_Reset();

    /* Wait for display ready */
    if (EPD_WaitUntilIdle(1000) != EPD_OK) {
        return EPD_BUSY_TIMEOUT;
    }

    /* Software reset */
    EPD_SendCommand(SSD1681_SW_RESET);
    if (EPD_WaitUntilIdle(1000) != EPD_OK) {
        return EPD_BUSY_TIMEOUT;
    }

    /* Driver output control */
    EPD_SendCommand(SSD1681_DRIVER_OUTPUT);
    EPD_SendData((EPD_HEIGHT - 1) & 0xFF);
    EPD_SendData(((EPD_HEIGHT - 1) >> 8) & 0x01);
    EPD_SendData(0x00);  /* Gate scanning sequence: G0-G199 */

    /* Data entry mode setting */
    EPD_SendCommand(SSD1681_DATA_ENTRY_MODE);
    EPD_SendData(0x03);  /* X increment, Y increment, update X first */

    /* Set RAM window */
    EPD_SetWindow(0, 0, EPD_WIDTH - 1, EPD_HEIGHT - 1);

    /* Border waveform control */
    EPD_SendCommand(SSD1681_BORDER_WAVEFORM);
    EPD_SendData(0x05);  /* VBD: GS transition, follow LUT */

    /* Temperature sensor control */
    EPD_SendCommand(SSD1681_TEMP_SENSOR_CTRL);
    EPD_SendData(0x80);  /* Internal temperature sensor */

    /* Set cursor to origin */
    EPD_SetCursor(0, 0);

    if (EPD_WaitUntilIdle(1000) != EPD_OK) {
        return EPD_BUSY_TIMEOUT;
    }

    return EPD_OK;
}

/**
 * @brief Trigger full display update
 */
void EPD_Update(void)
{
    EPD_SendCommand(SSD1681_DISPLAY_UPDATE_2);
    EPD_SendData(0xF7);  /* Full update sequence */
    EPD_SendCommand(SSD1681_MASTER_ACTIVATION);
    EPD_WaitUntilIdle(5000);  /* Full update takes ~2-3 seconds */
}

/**
 * @brief Display image buffer on screen
 * @param image Pointer to image buffer (5000 bytes for 200x200)
 */
void EPD_Display(uint8_t *image)
{
    /* Set RAM window and cursor */
    EPD_SetWindow(0, 0, EPD_WIDTH - 1, EPD_HEIGHT - 1);
    EPD_SetCursor(0, 0);

    /* Write to RAM */
    EPD_SendCommand(SSD1681_WRITE_RAM_BW);
    EPD_SendDataBurst(image, EPD_BUFFER_SIZE);

    /* Trigger update */
    EPD_Update();
}

/**
 * @brief Clear display with specified color
 * @param color EPD_WHITE (0xFF) or EPD_BLACK (0x00)
 */
void EPD_Clear(uint8_t color)
{
    /* Set RAM window and cursor */
    EPD_SetWindow(0, 0, EPD_WIDTH - 1, EPD_HEIGHT - 1);
    EPD_SetCursor(0, 0);

    /* Write color to entire RAM */
    EPD_SendCommand(SSD1681_WRITE_RAM_BW);
    EPD_DC_HIGH();
    EPD_CS_LOW();
    for (uint32_t i = 0; i < EPD_BUFFER_SIZE; i++) {
        HAL_SPI_Transmit(epd_spi, &color, 1, 10);
    }
    EPD_CS_HIGH();

    /* Trigger update */
    EPD_Update();
}

/**
 * @brief Put display in deep sleep mode
 */
void EPD_DeepSleep(void)
{
    EPD_SendCommand(SSD1681_DEEP_SLEEP);
    EPD_SendData(0x01);  /* Deep sleep mode 1 */
}

/**
 * @brief Fill buffer with color
 */
void EPD_FillBuffer(uint8_t *buffer, uint8_t color)
{
    memset(buffer, color, EPD_BUFFER_SIZE);
}

/**
 * @brief Set a single pixel in buffer
 * @note For 200x200 display, x: 0-199, y: 0-199
 */
void EPD_SetPixel(uint8_t *buffer, uint16_t x, uint16_t y, uint8_t color)
{
    if (x >= EPD_WIDTH || y >= EPD_HEIGHT) {
        return;
    }

    /* Calculate byte position and bit within byte */
    uint32_t byte_idx = (y * (EPD_WIDTH / 8)) + (x / 8);
    uint8_t bit_mask = 0x80 >> (x % 8);

    if (color == EPD_BLACK) {
        buffer[byte_idx] &= ~bit_mask;  /* Clear bit = black */
    } else {
        buffer[byte_idx] |= bit_mask;   /* Set bit = white */
    }
}

/**
 * @brief Draw a line using Bresenham's algorithm
 */
void EPD_DrawLine(uint8_t *buffer, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t color)
{
    int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;

    while (1) {
        EPD_SetPixel(buffer, x0, y0, color);

        if (x0 == x1 && y0 == y1) break;

        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/**
 * @brief Draw rectangle outline
 */
void EPD_DrawRect(uint8_t *buffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color)
{
    EPD_DrawLine(buffer, x, y, x + w - 1, y, color);         /* Top */
    EPD_DrawLine(buffer, x, y + h - 1, x + w - 1, y + h - 1, color);  /* Bottom */
    EPD_DrawLine(buffer, x, y, x, y + h - 1, color);         /* Left */
    EPD_DrawLine(buffer, x + w - 1, y, x + w - 1, y + h - 1, color);  /* Right */
}

/**
 * @brief Draw filled rectangle
 */
void EPD_FillRect(uint8_t *buffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color)
{
    for (uint16_t j = 0; j < h; j++) {
        for (uint16_t i = 0; i < w; i++) {
            EPD_SetPixel(buffer, x + i, y + j, color);
        }
    }
}

/**
 * @brief Draw a character at position
 */
void EPD_DrawChar(uint8_t *buffer, uint16_t x, uint16_t y, char c, uint8_t font_size, uint8_t color)
{
    if (c < 32 || c > 126) {
        c = ' ';  /* Replace unsupported chars with space */
    }

    uint8_t char_idx = c - 32;
    const uint8_t *font = EPD_GetFontData(font_size);
    uint8_t font_w = EPD_GetFontWidth(font_size);
    uint8_t font_h = EPD_GetFontHeight(font_size);

    if (font_size == 8) {
        /* 8x6 font: 6 bytes per char, each byte is a column */
        const uint8_t *char_data = &font[char_idx * font_w];

        for (uint8_t col = 0; col < font_w; col++) {
            uint8_t column_data = char_data[col];
            for (uint8_t row = 0; row < font_h; row++) {
                if (column_data & (1 << row)) {
                    EPD_SetPixel(buffer, x + col, y + row, color);
                } else {
                    /* Draw background for contrast */
                    EPD_SetPixel(buffer, x + col, y + row, color == EPD_BLACK ? EPD_WHITE : EPD_BLACK);
                }
            }
        }
    } else if (font_size == 16) {
        /* 16x8 font: 16 bytes per char (2 bytes per column, 8 columns) */
        const uint8_t *char_data = &font[char_idx * 16];

        for (uint8_t col = 0; col < font_w; col++) {
            uint16_t column_data = char_data[col * 2] | (char_data[col * 2 + 1] << 8);
            for (uint8_t row = 0; row < font_h; row++) {
                if (column_data & (1 << row)) {
                    EPD_SetPixel(buffer, x + col, y + row, color);
                } else {
                    EPD_SetPixel(buffer, x + col, y + row, color == EPD_BLACK ? EPD_WHITE : EPD_BLACK);
                }
            }
        }
    }
}

/**
 * @brief Draw string at position
 */
void EPD_DrawString(uint8_t *buffer, uint16_t x, uint16_t y, const char *str, uint8_t font_size, uint8_t color)
{
    uint8_t font_w = EPD_GetFontWidth(font_size);

    while (*str) {
        if (x + font_w > EPD_WIDTH) {
            /* Wrap to next line */
            x = 0;
            y += EPD_GetFontHeight(font_size);
        }
        if (y + EPD_GetFontHeight(font_size) > EPD_HEIGHT) {
            break;  /* No more space */
        }

        EPD_DrawChar(buffer, x, y, *str, font_size, color);
        x += font_w + 1;  /* 1 pixel spacing */
        str++;
    }
}

/**
 * @brief Draw number at position
 */
void EPD_DrawNumber(uint8_t *buffer, uint16_t x, uint16_t y, int32_t num, uint8_t font_size, uint8_t color)
{
    char str[12];
    int i = 0;
    int32_t n = num;

    if (num < 0) {
        str[i++] = '-';
        n = -num;
    }

    if (n == 0) {
        str[i++] = '0';
    } else {
        /* Find number of digits */
        int32_t temp = n;
        int digits = 0;
        while (temp > 0) {
            digits++;
            temp /= 10;
        }

        /* Convert to string */
        i += digits;
        int j = i - 1;
        while (n > 0) {
            str[j--] = '0' + (n % 10);
            n /= 10;
        }
    }

    str[i] = '\0';
    EPD_DrawString(buffer, x, y, str, font_size, color);
}
