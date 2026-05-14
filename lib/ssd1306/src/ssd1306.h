#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"
#include "ssd1306_fonts.h"
#include "stdint.h"


#define SSD1306_I2C_ADDRESS ((uint8_t)0x3C)

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

typedef void *ssd1306_handle_t; 


esp_err_t ssd1306_init(ssd1306_handle_t dev);


ssd1306_handle_t ssd1306_create(i2c_port_t port, uint16_t dev_addr);


void ssd1306_delete(ssd1306_handle_t dev);


void ssd1306_fill_point(
    ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos, uint8_t chPoint
);


void ssd1306_fill_rectangle(
    ssd1306_handle_t dev, uint8_t chXpos1, uint8_t chYpos1, uint8_t chXpos2,
    uint8_t chYpos2, uint8_t chDot
);


void ssd1306_draw_char(
    ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos, uint8_t chChr,
    uint8_t chSize, uint8_t chMode
);


void ssd1306_draw_num(
    ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos, uint32_t chNum,
    uint8_t chLen, uint8_t chSize
);


void ssd1306_draw_1616char(
    ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos, uint8_t chChar
);


void ssd1306_draw_3216char(
    ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos, uint8_t chChar
);


void ssd1306_draw_bitmap(
    ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos, const uint8_t *pchBmp,
    uint8_t chWidth, uint8_t chHeight
);


void ssd1306_draw_line(
    ssd1306_handle_t dev, int16_t chXpos1, int16_t chYpos1, int16_t chXpos2,
    int16_t chYpos2
);


esp_err_t ssd1306_refresh_gram(ssd1306_handle_t dev);


void ssd1306_clear_screen(ssd1306_handle_t dev, uint8_t chFill);


void ssd1306_draw_string(
    ssd1306_handle_t dev, uint8_t chXpos, uint8_t chYpos,
    const uint8_t *pchString, uint8_t chSize, uint8_t chMode
);

void ssd1306_display_on(ssd1306_handle_t dev);
void ssd1306_display_off(ssd1306_handle_t dev);

#ifdef __cplusplus
}
#endif
