/*
 * ili9341_gfx.c
 *
 *  Created on: Jan 17, 2020
 *      Author: andrew
 */

// ----------------------------------------------------------------- includes --

#include "ili9341_gfx.h"
#include "stdlib.h"
#include "string.h" // memset()

// ---------------------------------------------------------- private defines --

/* nothing */

// ----------------------------------------------------------- private macros --

/* nothing */

// ----------------------------------------------------------- private types --

/* nothing */

// ------------------------------------------------------- exported variables --

ili9341_color_t const ILI9341_BLACK        = (ili9341_color_t)0x0000;
ili9341_color_t const ILI9341_NAVY         = (ili9341_color_t)0x000F;
ili9341_color_t const ILI9341_DARKGREEN    = (ili9341_color_t)0x03E0;
ili9341_color_t const ILI9341_DARKCYAN     = (ili9341_color_t)0x03EF;
ili9341_color_t const ILI9341_MAROON       = (ili9341_color_t)0x7800;
ili9341_color_t const ILI9341_PURPLE       = (ili9341_color_t)0x780F;
ili9341_color_t const ILI9341_OLIVE        = (ili9341_color_t)0x7BE0;
ili9341_color_t const ILI9341_LIGHTGREY    = (ili9341_color_t)0xC618;
ili9341_color_t const ILI9341_DARKGREY     = (ili9341_color_t)0x7BEF;
ili9341_color_t const ILI9341_BLUE         = (ili9341_color_t)0x001F;
ili9341_color_t const ILI9341_GREEN        = (ili9341_color_t)0x07E0;
ili9341_color_t const ILI9341_CYAN         = (ili9341_color_t)0x07FF;
ili9341_color_t const ILI9341_RED          = (ili9341_color_t)0xF800;
ili9341_color_t const ILI9341_MAGENTA      = (ili9341_color_t)0xF81F;
ili9341_color_t const ILI9341_YELLOW       = (ili9341_color_t)0xFFE0;
ili9341_color_t const ILI9341_WHITE        = (ili9341_color_t)0xFFFF;
ili9341_color_t const ILI9341_ORANGE       = (ili9341_color_t)0xFD20;
ili9341_color_t const ILI9341_GREENYELLOW  = (ili9341_color_t)0xAFE5;
ili9341_color_t const ILI9341_PINK         = (ili9341_color_t)0xF81F;

// ------------------------------------------------------- private variables --

/* nothing */

// ------------------------------------------------------ function prototypes --

static void ili9341_spi_tft_set_address_rect(ili9341_device_t *dev,
    uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
static ili9341_bool_t ili9341_clip_rect(ili9341_device_t *dev,
    uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h);

// ------------------------------------------------------- exported functions --

void ili9341_draw_pixel(ili9341_device_t *dev, ili9341_color_t color,
    uint16_t x, uint16_t y)
{
  if (ibNOT(ili9341_clip_rect(dev, &x, &y, NULL, NULL)))
    { return; }

  uint16_t color_le = __U16_LEND(&color);

  // select target region
  ili9341_spi_tft_set_address_rect(dev, x, y, x + 1, y + 1);

  ili9341_spi_tft_select(dev);

  HAL_GPIO_WritePin(dev->data_command_port, dev->data_command_pin, __GPIO_PIN_SET__);
  HAL_SPI_Transmit(dev->spi_hal, (uint8_t *)&color_le, 2U, __SPI_MAX_DELAY__);

  ili9341_spi_tft_release(dev);
}

void ili9341_draw_line(ili9341_device_t *dev, ili9341_color_t color,
    uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  int32_t dx = (int32_t)x1 - x0;
  int32_t dy = (int32_t)y1 - y0;

  uint16_t x, y;
  int32_t err;
  int32_t step;

  if (0 == dx) {
    // vertical line
    if (0 == dy)
      { return; } // distance = 0, no line to draw
    ili9341_fill_rect(dev, color, x0, y0, 1, abs(dy));
    return;
  }
  else if (0 == dy) {
    // horizontal line
    if (0 == dx)
      { return; } // distance = 0, no line to draw
    ili9341_fill_rect(dev, color, x0, y0, abs(dx), 1);
    return;
  }

  ili9341_bool_t is_steep = abs(dy) > abs(dx);
  if (is_steep) {
    __SWAP(uint16_t, x0, y0);
    __SWAP(uint16_t, x1, y1);
  }

  if (x0 > x1) {
    __SWAP(uint16_t, x0, x1);
    __SWAP(uint16_t, y0, y1);
  }

  dx = x1 - x0;
  dy = abs(y1 - y0);
  err = dx >> 1;

  if (y0 < y1)
    { step = 1; }
  else
    { step = -1; }

  while (x0 <= x1) {

    if (is_steep)
      { x = y0; y = x0; }
    else
      { x = x0; y = y0; }

    // continue algorithm even if current pixel is outside of screen
    // bounds, so that the line is drawn at correct position once
    // it actually enters screen bounds (if ever).
    if ( (x >= 0) && (x <= dev->screen_size.width) &&
         (y >= 0) && (y <= dev->screen_size.height) ) {
      ili9341_draw_pixel(dev, color, x, y);
    }

    err -= dy;
    if (err < 0) {
      y0 += step;
      err += dx;
    }

    ++x0;
  }
}

void ili9341_draw_rect(ili9341_device_t *dev, ili9341_color_t color,
    uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  ili9341_draw_line(dev, color,     x,     y,   x+w,     y );
  ili9341_draw_line(dev, color,     x, y+h-1,   x+w, y+h-1 );
  ili9341_draw_line(dev, color,     x,     y,     x,   y+h );
  ili9341_draw_line(dev, color, x+w-1,     y, x+w-1,   y+h );
}

void ili9341_fill_rect(ili9341_device_t *dev, ili9341_color_t color,
    uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  // verify we have something within screen dimensions to be drawn
  if (ibNOT(ili9341_clip_rect(dev, &x, &y, &w, &h)))
    { return; }

  // 16-bit color, so need 2 bytes for each pixel being filled
  uint32_t num_pixels = w * h;
  uint32_t rect_sz    = 2 * num_pixels;

  // allocate largest block required to define rect
  uint32_t block_sz = rect_sz;
  if (block_sz > __SPI_TX_BLOCK_SZ__)
    { block_sz = __SPI_TX_BLOCK_SZ__; }
  uint8_t *block = malloc(block_sz);

  // fill entire block with ordered color data
  uint8_t msb = __U16_MSBYTE(color);
  uint8_t lsb = __U16_LSBYTE(color);
  for (uint16_t i = 0; i < block_sz; i += 2) {
    block[i + 0] = msb;
    block[i + 1] = lsb;
  }

  // select target region
  ili9341_spi_tft_set_address_rect(dev, x, y, (x + w - 1), (y + h - 1));
  ili9341_spi_tft_select(dev);

  // repeatedly send MIN(remaining-size, block-size) bytes of color data until
  // all rect bytes have been sent.
  HAL_GPIO_WritePin(dev->data_command_port, dev->data_command_pin, __GPIO_PIN_SET__);
  uint32_t curr_sz;
  while (rect_sz > 0) {
    curr_sz = rect_sz;
    if (curr_sz > block_sz)
      { curr_sz = block_sz; }
    HAL_SPI_Transmit(dev->spi_hal, block, curr_sz, __SPI_MAX_DELAY__);
    rect_sz -= curr_sz;
  }

  free(block);
  ili9341_spi_tft_release(dev);
}

void ili9341_fill_screen(ili9341_device_t *dev, ili9341_color_t color)
{
  ili9341_fill_rect(dev, color,
      0, 0, dev->screen_size.width, dev->screen_size.height);
}

void ili9341_draw_char(ili9341_device_t *dev, ili9341_text_attr_t attr, char ch)
{
  // verify we have something within screen dimensions to be drawn
  uint16_t _x = attr.origin_x;
  uint16_t _y = attr.origin_y;
  uint16_t _w = attr.font->width;
  uint16_t _h = attr.font->height;
  if (ibNOT(ili9341_clip_rect(dev, &_x, &_y, &_w, &_h)))
    { return; }

  // 16-bit color, so need 2 bytes for each pixel being filled
  uint32_t num_pixels = attr.font->width * attr.font->height;
  uint32_t rect_sz    = 2 * num_pixels;

  uint8_t fg_msb = __U16_MSBYTE(attr.fg_color);
  uint8_t fg_lsb = __U16_LSBYTE(attr.fg_color);
  uint8_t bg_msb = __U16_MSBYTE(attr.bg_color);
  uint8_t bg_lsb = __U16_LSBYTE(attr.bg_color);
  uint8_t msb, lsb;

  // allocate largest block required to define rect
  uint32_t block_sz = rect_sz;
  if (block_sz > __SPI_TX_BLOCK_SZ__)
    { block_sz = __SPI_TX_BLOCK_SZ__; }
  uint8_t *block = malloc(block_sz);

  ili9341_fill_rect(dev,
      attr.bg_color, attr.origin_x, attr.origin_y, attr.font->width, attr.font->height);

  // initialize the buffer with glyph from selected font
  uint8_t ch_index = glyph_index(ch);
  for (uint32_t yi = 0; yi < attr.font->height; ++yi) {
    uint32_t gl = (uint32_t)attr.font->glyph[ch_index * attr.font->height + yi];
    for (uint32_t xi = 0; xi < attr.font->width; ++xi) {
      if ((gl << xi) & 0x8000) {
        msb = fg_msb;
        lsb = fg_lsb;
      } else {
        msb = bg_msb;
        lsb = bg_lsb;
      }
      block[2 * (yi * attr.font->width + xi) + 0] = msb;
      block[2 * (yi * attr.font->width + xi) + 1] = lsb;
    }
  }

  // select target region
  ili9341_spi_tft_set_address_rect(dev,
      attr.origin_x, attr.origin_y,
      attr.origin_x + attr.font->width - 1, attr.origin_y + attr.font->height - 1);

  ili9341_spi_tft_select(dev);

  // transmit the character data in a single block transfer
  HAL_GPIO_WritePin(dev->data_command_port, dev->data_command_pin, __GPIO_PIN_SET__);
  HAL_SPI_Transmit(dev->spi_hal, block, block_sz, __SPI_MAX_DELAY__);

  free(block);
  ili9341_spi_tft_release(dev);
}

void ili9341_draw_string(ili9341_device_t *dev, ili9341_text_attr_t attr, char str[])
{
  uint16_t curr_x = attr.origin_x;
  uint16_t curr_y = attr.origin_y;

  while ('\0' != *str) {
    if ( (curr_x > dev->screen_size.width) ||
         (curr_y > dev->screen_size.height) )
      { break; }

    attr.origin_x = curr_x;
    attr.origin_y = curr_y;

    ili9341_draw_char(dev, attr, *str);

    curr_x += attr.font->width;
    ++str;
  }
}

// ------------------------------------------------------- private functions --

static void ili9341_spi_tft_set_address_rect(ili9341_device_t *dev,
    uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  ili9341_spi_tft_select(dev);

  // column address set
  ili9341_spi_write_command_data(dev, issNONE,
      0x2A, 4, (uint8_t[]){ (x0 >> 8) & 0xFF, x0 & 0xFF,
                            (x1 >> 8) & 0xFF, x1 & 0xFF });

  // row address set
  ili9341_spi_write_command_data(dev, issNONE,
      0x2B, 4, (uint8_t[]){ (y0 >> 8) & 0xFF, y0 & 0xFF,
                            (y1 >> 8) & 0xFF, y1 & 0xFF });

  // write to RAM
  ili9341_spi_write_command(dev, issNONE, 0x2C);

  ili9341_spi_tft_release(dev);
}

static ili9341_bool_t ili9341_clip_rect(ili9341_device_t *dev,
    uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h)
{
  // adjust bounds to clip any area outside screen dimensions:
  //    NOTE: x,y type is unsigned, so their value will never be < 0

  //  1. rect origin beyond screen dimensions, nothing to draw
  if ((*x >= dev->screen_size.width) || (*y >= dev->screen_size.height))
    { return ibFalse; }

  if ((NULL != w) && (NULL != h)) {

    //  2. rect width or height is 0, nothing to draw
    if ((0U == *w) || (0U == *h))
      { return ibFalse; }

    //  3. rect width beyond screen width, reduce rect width
    if ((*x + *w - 1) >= dev->screen_size.width)
      { *w = dev->screen_size.width - *x; }

    //  4. rect height beyond screen height, reduce rect height
    if ((*y + *h - 1) >= dev->screen_size.height)
      { *h = dev->screen_size.height - *y; }

    return (*w > 0U) && (*h > 0U);
  }

  return ibTrue;
}
