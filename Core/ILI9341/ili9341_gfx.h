/*
 * ili9341_gfx.h
 *
 *  Created on: Jan 17, 2020
 *      Author: andrew
 */

#ifndef __ILI9341_GFX_H
#define __ILI9341_GFX_H

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------- includes --

#include <stdint.h>

#include "ili9341.h"
#include "ili9341_font.h"

// ------------------------------------------------------------------ defines --

/* nothing */

// ------------------------------------------------------------------- macros --

#define __ILI9341_COLOR565(r, g, b) \
    (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))

// ----------------------------------------------------------- exported types --

typedef uint16_t ili9341_color_t;

typedef enum
{
  iwwNONE = -1,
  iwwTruncate, // = 0
  iwwCharWrap, // = 1
  iwwWordWrap, // = 2
  iwwCOUNT     // = 3
}
ili9341_word_wrap_t;

// ------------------------------------------------------- exported variables --

extern ili9341_color_t const ILI9341_BLACK;
extern ili9341_color_t const ILI9341_BLUE;
extern ili9341_color_t const ILI9341_RED;
extern ili9341_color_t const ILI9341_GREEN;
extern ili9341_color_t const ILI9341_CYAN;
extern ili9341_color_t const ILI9341_MAGENTA;
extern ili9341_color_t const ILI9341_YELLOW;
extern ili9341_color_t const ILI9341_WHITE;

// ------------------------------------------------------- exported functions --

void ili9341_fill_rect(ili9341_device_t *dev, ili9341_color_t color,
    uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void ili9341_fill_screen(ili9341_device_t *dev, ili9341_color_t color);

void ili9341_draw_char(ili9341_device_t *dev, uint16_t x, uint16_t y,
    ili9341_font_t const *font, ili9341_color_t fg_color, ili9341_color_t bg_color,
    char ch);
void ili9341_draw_string(ili9341_device_t *dev, uint16_t x, uint16_t y,
    ili9341_font_t const *font, ili9341_color_t fg_color, ili9341_color_t bg_color,
    ili9341_word_wrap_t word_wrap, char str[]);

#ifdef __cplusplus
}
#endif

#endif /* __ILI9341_GFX_H */
