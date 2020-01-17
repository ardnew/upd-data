/*
 * ili9341.c
 *
 *  Created on: Nov 28, 2019
 *      Author: andrew
 */

// ----------------------------------------------------------------- includes --

#include <stdlib.h> // malloc()
#include <string.h> // memset()
#include <ctype.h>

#include "ili9341.h"
#include "ili9341_gfx.h"
#include "ili9341_font.h"

// ---------------------------------------------------------- private defines --

#define __ILI9341_TOUCH_NORM_SAMPLES__ 16U

// ----------------------------------------------------------- private macros --

#define __IS_SPI_SLAVE(s) (((s) > issNONE) && ((s) < issCOUNT))

#define __SLAVE_SELECT(d, s)  \
  if (__IS_SPI_SLAVE(s)) { ili9341_spi_slave_select((d), (s)); }

#define __SLAVE_RELEASE(d, s) \
  if (__IS_SPI_SLAVE(s)) { ili9341_spi_slave_release((d), (s)); }

// ------------------------------------------------------------ private types --

/* nothing */

// ------------------------------------------------------- exported variables --

/* nothing */

// -------------------------------------------------------- private variables --

#if defined(__ILI9341_STATIC_MEM_ALLOC__)
static uint8_t __spi_tx_block__[__SPI_TX_BLOCK_SZ__]; // do not use, theoretically broken
#endif

// ---------------------------------------------- private function prototypes --

static void ili9341_reset(ili9341_device_t *dev);
static void ili9341_initialize(ili9341_device_t *dev);
static ili9341_two_dimension_t ili9341_screen_size(
    ili9341_screen_orientation_t orientation);
static uint8_t ili9341_screen_rotation(
    ili9341_screen_orientation_t orientation);

static int32_t interp(int32_t x, int32_t x0, int32_t x1, int32_t y0, int32_t y1);
static float finterp(float x, float x0, float x1, float y0, float y1);
ili9341_two_dimension_t ili9341_clip_touch_coordinate(ili9341_device_t *dev,
    uint16_t x_pos, uint16_t y_pos);
ili9341_two_dimension_t ili9341_scale_touch_coordinate(ili9341_device_t *dev,
    uint16_t x_pos, uint16_t y_pos);

// ------------------------------------------------------- exported functions --

ili9341_device_t *ili9341_device_new(

    SPI_HandleTypeDef *spi_hal,

    GPIO_TypeDef *reset_port,        uint16_t reset_pin,
    GPIO_TypeDef *tft_select_port,   uint16_t tft_select_pin,
    GPIO_TypeDef *data_command_port, uint16_t data_command_pin,

    ili9341_screen_orientation_t orientation,

    GPIO_TypeDef *touch_select_port, uint16_t touch_select_pin,
    GPIO_TypeDef *touch_irq_port,    uint16_t touch_irq_pin,

    ili9341_touch_support_t   touch_support,
    ili9341_touch_normalize_t touch_normalize,
    uint16_t touch_coordinate_min_x,
    uint16_t touch_coordinate_min_y,
    uint16_t touch_coordinate_max_x,
    uint16_t touch_coordinate_max_y)
{
  ili9341_device_t *dev = NULL;

  if (NULL != spi_hal) {

    if ( (NULL != reset_port)        && IS_GPIO_PIN(reset_pin)         &&
         (NULL != tft_select_port)   && IS_GPIO_PIN(tft_select_pin)    &&
         (NULL != data_command_port) && IS_GPIO_PIN(data_command_pin)  &&
         (orientation > isoNONE)     && (orientation < isoCOUNT)       ) {

      // we must either NOT support the touch interface, OR we must have valid
      // touch interface parameters
      if ( itsSupported != touch_support ||
           ( (NULL != touch_select_port) && IS_GPIO_PIN(touch_select_pin) &&
             (NULL != touch_irq_port)    && IS_GPIO_PIN(touch_irq_pin)    &&
             (touch_coordinate_min_x < touch_coordinate_max_x)            &&
             (touch_coordinate_min_y < touch_coordinate_max_y)            )) {

        if (NULL != (dev = malloc(sizeof(ili9341_device_t)))) {

          dev->spi_hal              = spi_hal;

          dev->reset_port           = reset_port;
          dev->reset_pin            = reset_pin;
          dev->tft_select_port      = tft_select_port;
          dev->tft_select_pin       = tft_select_pin;
          dev->data_command_port    = data_command_port;
          dev->data_command_pin     = data_command_pin;

          dev->orientation          = orientation;
          dev->screen_size          = ili9341_screen_size(orientation);

          if (touch_support) {

            dev->touch_select_port    = touch_select_port;
            dev->touch_select_pin     = touch_select_pin;
            dev->touch_irq_port       = touch_irq_port;
            dev->touch_irq_pin        = touch_irq_pin;

            dev->touch_support        = touch_support;
            dev->touch_normalize      = touch_normalize;
            dev->touch_coordinate     = (ili9341_two_dimension_t){ {0U}, {0U} };
            dev->touch_coordinate_min = (ili9341_two_dimension_t){
                {touch_coordinate_min_x}, {touch_coordinate_min_y} };
            dev->touch_coordinate_max = (ili9341_two_dimension_t){
                {touch_coordinate_max_x}, {touch_coordinate_max_y} };

            dev->touch_pressed        = itpNotPressed;
            dev->touch_pressed_begin  = NULL;
            dev->touch_pressed_end    = NULL;

          } else {

            dev->touch_select_port    = NULL;
            dev->touch_select_pin     = 0;
            dev->touch_irq_port       = NULL;
            dev->touch_irq_pin        = 0;

            dev->touch_support        = touch_support;
            dev->touch_normalize      = itnNONE;
            dev->touch_coordinate     = (ili9341_two_dimension_t){ {0U}, {0U} };
            dev->touch_coordinate_min = (ili9341_two_dimension_t){ {0U}, {0U} };
            dev->touch_coordinate_max = (ili9341_two_dimension_t){ {0U}, {0U} };

            dev->touch_pressed        = itpNONE;
            dev->touch_pressed_begin  = NULL;
            dev->touch_pressed_end    = NULL;
          }

          ili9341_initialize(dev);
          ili9341_fill_screen(dev, ILI9341_BLACK);
        }
      }
    }
  }

  return dev;
}

void ili9341_touch_interrupt(ili9341_device_t *dev)
{
  uint16_t x_pos;
  uint16_t y_pos;

  // read the new/incoming state of the touch screen
  ili9341_touch_pressed_t pressed =
      ili9341_touch_coordinate(dev, &x_pos, &y_pos);

  // switch path based on existing/prior state of the touch screen. note this
  // requires the touch interrupt GPIO EXTI be set to detect both falling and
  // rising edges.
  switch (dev->touch_pressed) {
    case itpNONE:
    case itpNotPressed:
      if (itpPressed == pressed) {
        // state change, start of press
        if (NULL != dev->touch_pressed_begin) {
          // use the current, normalized touch coordinate
          dev->touch_pressed_begin(dev, x_pos, y_pos);
        }
      }
      break;

    case itpPressed:
      if (itpNotPressed == pressed) {
        // state change, end of press
        if (NULL != dev->touch_pressed_end) {
          // use the last-known valid touch coordinate, since the current
          // state does not have a valid touch.
          dev->touch_pressed_end(dev,
              dev->touch_coordinate.x, dev->touch_coordinate.y);
        }
      }
      break;

    default:
      break;
  }

  // update the internal state with current state of touch screen
  if (pressed != dev->touch_pressed) {
    dev->touch_pressed = pressed;
  }

  if (itpPressed == pressed) {
    dev->touch_coordinate.x = x_pos;
    dev->touch_coordinate.y = y_pos;
  }
}

ili9341_touch_pressed_t ili9341_touch_pressed(ili9341_device_t *dev)
{
  if (NULL == dev)
    { return itpNONE; }

  if (__GPIO_PIN_CLR__ == HAL_GPIO_ReadPin(dev->touch_irq_port, dev->touch_irq_pin))
    { return itpPressed; }
  else
    { return itpNotPressed; }
}

void ili9341_set_touch_pressed_begin(ili9341_device_t *dev, ili9341_touch_callback_t callback)
{
  if ((NULL != dev) && (NULL != callback)) {
    dev->touch_pressed_begin = callback;
  }
}

void ili9341_set_touch_pressed_end(ili9341_device_t *dev, ili9341_touch_callback_t callback)
{
  if ((NULL != dev) && (NULL != callback)) {
    dev->touch_pressed_end = callback;
  }
}

ili9341_touch_pressed_t ili9341_touch_coordinate(ili9341_device_t *dev,
    uint16_t *x_pos, uint16_t *y_pos)
{
  if (NULL == dev)
    { return itpNONE; }

  uint16_t req_samples;
  switch (dev->touch_normalize) {
    default:
    case itnNotNormalized:
      req_samples = 1;
      break;
    case itnNormalized:
      req_samples = __ILI9341_TOUCH_NORM_SAMPLES__;
      break;
  }

  // touch coordinates returned as 16-bit values
  static uint8_t y_cmd[] = { 0x93 };
  static uint8_t x_cmd[] = { 0xD3 };
  static uint8_t sleep[] = { 0x00 };

  uint32_t x_avg = 0U;
  uint32_t y_avg = 0U;

  uint16_t sample = req_samples;
  uint16_t num_samples = 0U;

  MODIFY_REG(dev->spi_hal->Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_128);

  ili9341_spi_touch_select(dev);

  while ((itpPressed == ili9341_touch_pressed(dev)) && (sample--)) {

     HAL_SPI_Transmit(dev->spi_hal, (uint8_t*)y_cmd, sizeof(y_cmd), __SPI_MAX_DELAY__);
     uint8_t y_raw[2];
     HAL_SPI_TransmitReceive(dev->spi_hal, (uint8_t*)y_cmd, y_raw, sizeof(y_raw), __SPI_MAX_DELAY__);

     HAL_SPI_Transmit(dev->spi_hal, (uint8_t*)x_cmd, sizeof(x_cmd), __SPI_MAX_DELAY__);
     uint8_t x_raw[2];
     HAL_SPI_TransmitReceive(dev->spi_hal, (uint8_t*)x_cmd, x_raw, sizeof(x_raw), __SPI_MAX_DELAY__);

     x_avg += (((uint16_t)x_raw[0]) << 8) | ((uint16_t)x_raw[1]);
     y_avg += (((uint16_t)y_raw[0]) << 8) | ((uint16_t)y_raw[1]);

    ++num_samples;
  }
  HAL_SPI_Transmit(dev->spi_hal, (uint8_t*)sleep, sizeof(sleep), __SPI_MAX_DELAY__);

  ili9341_spi_touch_release(dev);

  MODIFY_REG(dev->spi_hal->Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_8);

  if (num_samples < req_samples)
    { return itpNotPressed; }

  ili9341_two_dimension_t coord =
      ili9341_scale_touch_coordinate(dev, x_avg / req_samples, y_avg / req_samples);

  *x_pos = coord.x;
  *y_pos = coord.y;

  return itpPressed;
}

uint32_t ili9341_alloc_spi_tx_block(ili9341_device_t *dev,
    uint32_t tx_block_sz, uint8_t **tx_block)
{
  if (tx_block_sz > __SPI_TX_BLOCK_SZ__)
    { tx_block_sz = __SPI_TX_BLOCK_SZ__; }

#if defined(__ILI9341_STATIC_MEM_ALLOC__)
  *tx_block = __spi_tx_block__;
#else
  *tx_block = malloc(tx_block_sz);
#endif

  return tx_block_sz;
}

void ili9341_free_spi_tx_block(ili9341_device_t *dev,
    uint8_t **tx_block)
{
#if defined(__ILI9341_STATIC_MEM_ALLOC__)
  ; /* nothing */
#else
  free(*tx_block);
#endif
}

void ili9341_init_spi_tx_block(ili9341_device_t *dev,
    uint32_t tx_block_sz, uint8_t **tx_block)
{
  if (NULL != *tx_block) {
    for (size_t i = 0U; i < tx_block_sz; ++i)
      { (*tx_block)[i] = 0U; }
  }
}

void ili9341_spi_tft_select(ili9341_device_t *dev)
{
  // clear bit indicates the TFT is -active- slave SPI device
  HAL_GPIO_WritePin(dev->tft_select_port, dev->tft_select_pin, __GPIO_PIN_CLR__);
}

void ili9341_spi_tft_release(ili9341_device_t *dev)
{
  // set bit indicates the TFT is -inactive- slave SPI device
  HAL_GPIO_WritePin(dev->tft_select_port, dev->tft_select_pin, __GPIO_PIN_SET__);
}

void ili9341_spi_touch_select(ili9341_device_t *dev)
{
  // clear bit indicates the touch screen is -active- slave SPI device
  HAL_GPIO_WritePin(dev->touch_select_port, dev->touch_select_pin, __GPIO_PIN_CLR__);
}

void ili9341_spi_touch_release(ili9341_device_t *dev)
{
  // set bit indicates the touch screen is -inactive- slave SPI device
  HAL_GPIO_WritePin(dev->touch_select_port, dev->touch_select_pin, __GPIO_PIN_SET__);
}

void ili9341_spi_slave_select(ili9341_device_t *dev,
    ili9341_spi_slave_t spi_slave)
{
  switch (spi_slave) {
    case issDisplayTFT:  ili9341_spi_tft_select(dev);   break;
    case issTouchScreen: ili9341_spi_touch_select(dev); break;
    default: break;
  }
}

void ili9341_spi_slave_release(ili9341_device_t *dev,
    ili9341_spi_slave_t spi_slave)
{
  switch (spi_slave) {

    case issDisplayTFT:  ili9341_spi_tft_release(dev);   break;
    case issTouchScreen: ili9341_spi_touch_release(dev); break;
    default: break;
  }
}

void ili9341_spi_write_command(ili9341_device_t *dev,
    ili9341_spi_slave_t spi_slave, uint8_t command)
{
  __SLAVE_SELECT(dev, spi_slave);

  HAL_GPIO_WritePin(dev->data_command_port, dev->data_command_pin, __GPIO_PIN_CLR__);
  HAL_SPI_Transmit(dev->spi_hal, &command, sizeof(command), __SPI_MAX_DELAY__);

  __SLAVE_RELEASE(dev, spi_slave);
}

void ili9341_spi_write_data(ili9341_device_t *dev,
    ili9341_spi_slave_t spi_slave, uint16_t data_sz, uint8_t data[])
{
  __SLAVE_SELECT(dev, spi_slave);

  HAL_GPIO_WritePin(dev->data_command_port, dev->data_command_pin, __GPIO_PIN_SET__);
  HAL_SPI_Transmit(dev->spi_hal, data, data_sz, __SPI_MAX_DELAY__);

  __SLAVE_RELEASE(dev, spi_slave);
}

void ili9341_spi_write_data_read(ili9341_device_t *dev,
    ili9341_spi_slave_t spi_slave,
    uint16_t data_sz, uint8_t tx_data[], uint8_t rx_data[])
{
  __SLAVE_SELECT(dev, spi_slave);

  HAL_GPIO_WritePin(dev->data_command_port, dev->data_command_pin, __GPIO_PIN_SET__);
  HAL_SPI_TransmitReceive(dev->spi_hal, tx_data, rx_data, data_sz, __SPI_MAX_DELAY__);

  __SLAVE_RELEASE(dev, spi_slave);
}

void ili9341_spi_write_command_data(ili9341_device_t *dev,
    ili9341_spi_slave_t spi_slave, uint8_t command, uint16_t data_sz, uint8_t data[])
{
  __SLAVE_SELECT(dev, spi_slave);

  ili9341_spi_write_command(dev, issNONE, command);
  ili9341_spi_write_data(dev, issNONE, data_sz, data);

  __SLAVE_RELEASE(dev, spi_slave);
}

// -------------------------------------------------------- private functions --

static void ili9341_reset(ili9341_device_t *dev)
{
  // the reset pin on ILI9341 is active low, so driving low temporarily will
  // reset the device (also resets the touch screen peripheral)
  HAL_GPIO_WritePin(dev->reset_port, dev->reset_pin, __GPIO_PIN_CLR__);
  HAL_Delay(200);
  HAL_GPIO_WritePin(dev->reset_port, dev->reset_pin, __GPIO_PIN_SET__);

  // ensure both slave lines are open
  ili9341_spi_tft_release(dev);
  ili9341_spi_touch_release(dev);
}

static void ili9341_initialize(ili9341_device_t *dev)
{
  ili9341_reset(dev);
  ili9341_spi_tft_select(dev);

  // command list is based on https://github.com/martnak/STM32-ILI9341

  // SOFTWARE RESET
  ili9341_spi_write_command(dev, issNONE, 0x01);
  HAL_Delay(1000);

  // POWER CONTROL A
  ili9341_spi_write_command_data(dev, issNONE,
      0xCB, 5, (uint8_t[]){ 0x39, 0x2C, 0x00, 0x34, 0x02 });

  // POWER CONTROL B
  ili9341_spi_write_command_data(dev, issNONE,
      0xCF, 3, (uint8_t[]){ 0x00, 0xC1, 0x30 });

  // DRIVER TIMING CONTROL A
  ili9341_spi_write_command_data(dev, issNONE,
      0xE8, 3, (uint8_t[]){ 0x85, 0x00, 0x78 });

  // DRIVER TIMING CONTROL B
  ili9341_spi_write_command_data(dev, issNONE,
      0xEA, 2, (uint8_t[]){ 0x00, 0x00 });

  // POWER ON SEQUENCE CONTROL
  ili9341_spi_write_command_data(dev, issNONE,
      0xED, 4, (uint8_t[]){ 0x64, 0x03, 0x12, 0x81 });

  // PUMP RATIO CONTROL
  ili9341_spi_write_command_data(dev, issNONE,
      0xF7, 1, (uint8_t[]){ 0x20 });

  // POWER CONTROL,VRH[5:0]
  ili9341_spi_write_command_data(dev, issNONE,
      0xC0, 1, (uint8_t[]){ 0x23 });

  // POWER CONTROL,SAP[2:0];BT[3:0]
  ili9341_spi_write_command_data(dev, issNONE,
      0xC1, 1, (uint8_t[]){ 0x10 });

  // VCM CONTROL
  ili9341_spi_write_command_data(dev, issNONE,
      0xC5, 2, (uint8_t[]){ 0x3E, 0x28 });

  // VCM CONTROL 2
  ili9341_spi_write_command_data(dev, issNONE,
      0xC7, 1, (uint8_t[]){ 0x86 });

  // MEMORY ACCESS CONTROL
  ili9341_spi_write_command_data(dev, issNONE,
      0x36, 1, (uint8_t[]){ 0x48 });

  // PIXEL FORMAT
  ili9341_spi_write_command_data(dev, issNONE,
      0x3A, 1, (uint8_t[]){ 0x55 });

  // FRAME RATIO CONTROL, STANDARD RGB COLOR
  ili9341_spi_write_command_data(dev, issNONE,
      0xB1, 2, (uint8_t[]){ 0x00, 0x18 });

  // DISPLAY FUNCTION CONTROL
  ili9341_spi_write_command_data(dev, issNONE,
      0xB6, 3, (uint8_t[]){ 0x08, 0x82, 0x27 });

  // 3GAMMA FUNCTION DISABLE
  ili9341_spi_write_command_data(dev, issNONE,
      0xF2, 1, (uint8_t[]){ 0x00 });

  // GAMMA CURVE SELECTED
  ili9341_spi_write_command_data(dev, issNONE,
      0x26, 1, (uint8_t[]){ 0x01 });

  // POSITIVE GAMMA CORRECTION
  ili9341_spi_write_command_data(dev, issNONE,
      0xE0, 15, (uint8_t[]){ 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                             0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00 });

  // NEGATIVE GAMMA CORRECTION
  ili9341_spi_write_command_data(dev, issNONE,
      0xE1, 15, (uint8_t[]){ 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                             0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F });

  // EXIT SLEEP
  ili9341_spi_write_command(dev, issNONE, 0x11);
  HAL_Delay(120);

  // TURN ON DISPLAY
  ili9341_spi_write_command(dev, issNONE, 0x29);

  // MADCTL
  ili9341_spi_write_command_data(dev, issNONE,
      0x36, 1, (uint8_t[]){ ili9341_screen_rotation(dev->orientation) });

  ili9341_spi_tft_release(dev);
}

static ili9341_two_dimension_t ili9341_screen_size(
    ili9341_screen_orientation_t orientation)
{
  switch (orientation) {
    default:
    case isoDown:
      return (ili9341_two_dimension_t){ { .width = 240U }, { .height = 320U } };
    case isoRight:
      return (ili9341_two_dimension_t){ { .width = 320U }, { .height = 240U } };
    case isoUp:
      return (ili9341_two_dimension_t){ { .width = 240U }, { .height = 320U } };
    case isoLeft:
      return (ili9341_two_dimension_t){ { .width = 320U }, { .height = 240U } };
  }
}

static uint8_t ili9341_screen_rotation(
    ili9341_screen_orientation_t orientation)
{
  switch (orientation) {
    default:
    case isoDown:
      return 0x40 | 0x08;
    case isoRight:
      return 0x40 | 0x80 | 0x20 | 0x08;
    case isoUp:
      return 0x80 | 0x08;
    case isoLeft:
      return 0x20 | 0x08;
  }
}

static int32_t interp(int32_t x, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
  if (x1 == x0)
    { return 0; } // return 0 on divide-by-zero

  return (x - x0) * (y1 - y0) / (x1 - x0) + y0;
}

static float finterp(float x, float x0, float x1, float y0, float y1)
{
  if (__FABS(x1 - x0) < 5.e-6F)
    { return 0.0F; } // return 0 on divide-by-zero

  return (x - x0) * (y1 - y0) / (x1 - x0) + y0;
}

ili9341_two_dimension_t ili9341_clip_touch_coordinate(ili9341_device_t *dev,
    uint16_t x_pos, uint16_t y_pos)
{
  ili9341_two_dimension_t coord = { .x = x_pos, .y = y_pos };

  if (NULL != dev) {

    if (coord.x < dev->touch_coordinate_min.x)
      { coord.x = dev->touch_coordinate_min.x; }

    if (coord.x > dev->touch_coordinate_max.x)
      { coord.x = dev->touch_coordinate_max.x; }

    if (coord.y < dev->touch_coordinate_min.y)
      { coord.y = dev->touch_coordinate_min.y; }

    if (coord.y > dev->touch_coordinate_max.y)
      { coord.y = dev->touch_coordinate_max.y; }
  }

  return coord;
}

ili9341_two_dimension_t ili9341_scale_touch_coordinate(ili9341_device_t *dev,
    uint16_t x_pos, uint16_t y_pos)
{
  ili9341_two_dimension_t coord =
      ili9341_clip_touch_coordinate(dev, x_pos, y_pos);

  if (NULL != dev) {
    switch (dev->orientation) {
      default:
      case isoPortrait:
      case isoPortraitFlip:
        coord = (ili9341_two_dimension_t){
          .x = interp(coord.x,
              dev->touch_coordinate_min.x, dev->touch_coordinate_max.x,
              dev->screen_size.width, 0U),
          .y = interp(coord.y,
              dev->touch_coordinate_min.y, dev->touch_coordinate_max.y,
              dev->screen_size.height, 0U)
        };
        break;

      case isoLandscape:
      case isoLandscapeFlip:
        coord = (ili9341_two_dimension_t){
          .x = interp(coord.x,
              dev->touch_coordinate_min.x, dev->touch_coordinate_max.x,
              dev->screen_size.height, 0U),
          .y = interp(coord.y,
              dev->touch_coordinate_min.y, dev->touch_coordinate_max.y,
              dev->screen_size.width, 0U)
        };
        break;
    }
  }

  return coord;
}


