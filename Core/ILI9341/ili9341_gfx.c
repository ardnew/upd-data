/*
 * ili9341_gfx.c
 *
 *  Created on: Jan 17, 2020
 *      Author: andrew
 */

// ----------------------------------------------------------------- includes --

#include "ili9341_gfx.h"

// ---------------------------------------------------------- private defines --

/* nothing */

// ----------------------------------------------------------- private macros --

/* nothing */

// ----------------------------------------------------------- private types --

/* nothing */

// ------------------------------------------------------- exported variables --

ili9341_color_t const ILI9341_BLACK   = (ili9341_color_t)0x0000;
ili9341_color_t const ILI9341_BLUE    = (ili9341_color_t)0x001F;
ili9341_color_t const ILI9341_RED     = (ili9341_color_t)0xF800;
ili9341_color_t const ILI9341_GREEN   = (ili9341_color_t)0x07E0;
ili9341_color_t const ILI9341_CYAN    = (ili9341_color_t)0x07FF;
ili9341_color_t const ILI9341_MAGENTA = (ili9341_color_t)0xF81F;
ili9341_color_t const ILI9341_YELLOW  = (ili9341_color_t)0xFFE0;
ili9341_color_t const ILI9341_WHITE   = (ili9341_color_t)0xFFFF;

// ------------------------------------------------------- private variables --

/* nothing */

// ------------------------------------------------------ function prototypes --

static void ili9341_spi_tft_set_address_rect(ili9341_device_t *dev,
    uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

// ------------------------------------------------------- exported functions --

void ili9341_fill_rect(ili9341_device_t *dev, ili9341_color_t color,
    uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  // adjust bounds to clip any area outside screen dimensions:
  //  1. rect origin beyond screen dimensions, nothing to draw
  if ((x >= dev->screen_size.width) || (y >= dev->screen_size.height))
    { return; }
  //  2. rect width beyond screen width, reduce rect width
  if ((x + w - 1) >= dev->screen_size.width)
    { w = dev->screen_size.width - x; }
  //  3. rect height beyond screen height, reduce rect height
  if ((y + h - 1) >= dev->screen_size.height)
    { h = dev->screen_size.height - y; }

  // 16-bit color, so need 2 bytes for each pixel being filled
  uint32_t num_pixels = w * h;
  uint32_t rect_sz    = 2 * num_pixels;

  // allocate largest block required to define rect
  uint8_t *block = NULL;
  uint32_t block_sz = ili9341_alloc_spi_tx_block(dev, rect_sz, &block);

  // failed to allocate a usable block of memory
  if (0 == block_sz || NULL == block)
    { return; }

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

  // cleanup, be sure to free the potentially-massive buffer
  ili9341_free_spi_tx_block(dev, &block);
  ili9341_spi_tft_release(dev);
}

void ili9341_fill_screen(ili9341_device_t *dev, ili9341_color_t color)
{
  ili9341_fill_rect(dev, color,
      0, 0, dev->screen_size.width, dev->screen_size.height);
}

void ili9341_draw_char(ili9341_device_t *dev, uint16_t x, uint16_t y,
    ili9341_font_t const *font, ili9341_color_t fg_color, ili9341_color_t bg_color,
    char ch)
{
  // 16-bit color, so need 2 bytes for each pixel being filled
  uint32_t num_pixels = font->width * font->height;
  uint32_t rect_sz    = 2 * num_pixels;

  uint8_t fg_msb = __U16_MSBYTE(fg_color);
  uint8_t fg_lsb = __U16_LSBYTE(fg_color);
  uint8_t bg_msb = __U16_MSBYTE(bg_color);
  uint8_t bg_lsb = __U16_LSBYTE(bg_color);
  uint8_t msb, lsb;

  // allocate largest block required to define rect
  uint8_t *block = NULL;
  uint32_t block_sz = ili9341_alloc_spi_tx_block(dev, rect_sz, &block);

  // failed to allocate a usable block of memory
  if (0 == block_sz || NULL == block)
    { return; }

  ili9341_fill_rect(dev, bg_color, x, y, font->width, font->height);

  // initialize the buffer with glyph from selected font
  uint8_t ch_index = glyph_index(ch);
  for (uint32_t yi = 0; yi < font->height; ++yi) {
    uint32_t gl = (uint32_t)font->glyph[ch_index * font->height + yi];
    for (uint32_t xi = 0; xi < font->width; ++xi) {
      if ((gl << xi) & 0x8000) {
        msb = fg_msb;
        lsb = fg_lsb;
      } else {
        msb = bg_msb;
        lsb = bg_lsb;
      }
      block[2 * (yi * font->width + xi) + 0] = msb;
      block[2 * (yi * font->width + xi) + 1] = lsb;
    }
  }

  // select target region
  ili9341_spi_tft_set_address_rect(dev, x, y, x + font->width - 1, y + font->height - 1);
  ili9341_spi_tft_select(dev);

  // transmit the character data in a single block transfer
  HAL_GPIO_WritePin(dev->data_command_port, dev->data_command_pin, __GPIO_PIN_SET__);
  HAL_SPI_Transmit(dev->spi_hal, block, block_sz, __SPI_MAX_DELAY__);

  // cleanup, be sure to free the transmit buffer
  ili9341_free_spi_tx_block(dev, &block);
  ili9341_spi_tft_release(dev);
}

void ili9341_draw_string(ili9341_device_t *dev, uint16_t x, uint16_t y,
    ili9341_font_t const *font, ili9341_color_t fg_color, ili9341_color_t bg_color,
    ili9341_word_wrap_t word_wrap, char str[])
{
  uint16_t curr_x = x;
  uint16_t curr_y = y;

  while ('\0' != *str) {
    if (iwwTruncate == word_wrap) {
      if ( (curr_x > dev->screen_size.width) ||
           (curr_y > dev->screen_size.height) )
        { break; }
    }
    else if (iwwCharWrap == word_wrap) {
      /* TODO */
      break;
    }
    else if (iwwWordWrap == word_wrap) {
      /* TODO */
      break;
    }

    ili9341_draw_char(dev, curr_x, curr_y, font, fg_color, bg_color, *str);

    curr_x += font->width;
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
