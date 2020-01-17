/*
 * ili9341.h
 *
 *  Created on: Nov 28, 2019
 *      Author: andrew
 */

#ifndef __ILI9341_H
#define __ILI9341_H

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------- includes --

#if defined (STM32L476xx)
#include "stm32l4xx_hal.h"
#elif defined(STM32F072xx)
#include "stm32f0xx_hal.h"
#elif defined(STM32F401xx)
#include "stm32f4xx_hal.h"
#elif defined(STM32G431xx)
#include "stm32g4xx_hal.h"
#elif defined(STM32G031xx)
#include "stm32g0xx_hal.h"
#endif

// ------------------------------------------------------------------ defines --

#define __GPIO_PIN_CLR__    GPIO_PIN_RESET
#define __GPIO_PIN_SET__    GPIO_PIN_SET

#define __SPI_MAX_DELAY__   HAL_MAX_DELAY
#define __SPI_TX_BLOCK_SZ__ (1U * 1024U) // 1 KiB -MAX- for HAL SPI transmit

// whether memory should be allocated as-needed for the SPI block transfers, or
// if they should share a common pool allocated at compile-time. static
// allocation may be faster, but it's a waste of RAM if you aren't frequently
// doing full-screen refreshes.
//#define __ILI9341_STATIC_MEM_ALLOC__

// ------------------------------------------------------------------- macros --

#define __U16_MSBYTE(u) (uint8_t)(((uint16_t)(u) >> 8U) & 0xFF)
#define __U16_LSBYTE(u) (uint8_t)(((uint16_t)(u)      ) & 0xFF)

// convert value at addr to little-endian (16-bit)
#define __U16_LEND(addr)                                   \
    ( ( (((uint16_t)(*(((uint8_t *)(addr)) + 0)))      ) | \
        (((uint16_t)(*(((uint8_t *)(addr)) + 1))) << 8U) ) )

// convert value at addr to little-endian (32-bit)
#define __U32_LEND(addr)                                    \
    ( ( (((uint32_t)(*(((uint8_t *)(addr)) + 0)))       ) | \
        (((uint32_t)(*(((uint8_t *)(addr)) + 1))) <<  8U) | \
        (((uint32_t)(*(((uint8_t *)(addr)) + 2))) << 16U) | \
        (((uint32_t)(*(((uint8_t *)(addr)) + 3))) << 24U) ) )

#define __FABS(v) ((v) < 0.0F ? -(v) : (v))

// ----------------------------------------------------------- exported types --

typedef struct ili9341_device ili9341_device_t;

typedef struct
{
  union
  {
    uint16_t x;
    uint16_t width;
  };
  union
  {
    uint16_t y;
    uint16_t height;
  };
}
ili9341_two_dimension_t;

typedef enum
{
  // orientation is based on position of board pins when looking at the screen
  isoNONE                     = -1,
  isoDown,   isoPortrait      = isoDown,  // = 0
  isoRight,  isoLandscape     = isoRight, // = 1
  isoUp,     isoPortraitFlip  = isoUp,    // = 2
  isoLeft,   isoLandscapeFlip = isoLeft,  // = 3
  isoCOUNT                                // = 4
}
ili9341_screen_orientation_t;

typedef enum
{
  itsNONE = -1,
  itsNotSupported, // = 0
  itsSupported,    // = 1
  itsCOUNT         // = 2
}
ili9341_touch_support_t;

typedef enum
{
  itpNONE = -1,
  itpNotPressed, // = 0
  itpPressed,    // = 1
  itpCOUNT       // = 2
}
ili9341_touch_pressed_t;

typedef enum
{
  itnNONE = -1,
  itnNotNormalized, // = 0
  itnNormalized,    // = 1
  itnCOUNT          // = 2
}
ili9341_touch_normalize_t;

typedef enum
{
  issNONE = -1,
  issDisplayTFT,  // = 0
  issTouchScreen, // = 1
  issCOUNT        // = 2
}
ili9341_spi_slave_t;

typedef void (*ili9341_touch_callback_t)(ili9341_device_t *, uint16_t, uint16_t);

typedef HAL_StatusTypeDef ili9341_status_t;

struct ili9341_device
{
  SPI_HandleTypeDef *spi_hal;

  GPIO_TypeDef *reset_port;
  uint16_t      reset_pin;
  GPIO_TypeDef *tft_select_port;
  uint16_t      tft_select_pin;
  GPIO_TypeDef *data_command_port;
  uint16_t      data_command_pin;

  ili9341_screen_orientation_t orientation;
  ili9341_two_dimension_t      screen_size;

  GPIO_TypeDef *touch_select_port;
  uint16_t      touch_select_pin;
  GPIO_TypeDef *touch_irq_port;
  uint16_t      touch_irq_pin;

  ili9341_touch_support_t   touch_support;
  ili9341_touch_normalize_t touch_normalize;
  ili9341_two_dimension_t   touch_coordinate;
  ili9341_two_dimension_t   touch_coordinate_min;
  ili9341_two_dimension_t   touch_coordinate_max;

  ili9341_touch_pressed_t  touch_pressed;
  ili9341_touch_callback_t touch_pressed_begin;
  ili9341_touch_callback_t touch_pressed_end;
};

// ------------------------------------------------------- exported variables --

/* nothing */

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
    uint16_t touch_coordinate_max_y);

uint32_t ili9341_alloc_spi_tx_block(ili9341_device_t *dev,
    uint32_t tx_block_sz, uint8_t **tx_block);
void ili9341_free_spi_tx_block(ili9341_device_t *dev,
    uint8_t **tx_block);
void ili9341_init_spi_tx_block(ili9341_device_t *dev,
    uint32_t tx_block_sz, uint8_t **tx_block);

void ili9341_spi_tft_select(ili9341_device_t *dev);
void ili9341_spi_tft_release(ili9341_device_t *dev);
void ili9341_spi_touch_select(ili9341_device_t *dev);
void ili9341_spi_touch_release(ili9341_device_t *dev);
void ili9341_spi_slave_select(ili9341_device_t *dev,
    ili9341_spi_slave_t spi_slave);
void ili9341_spi_slave_release(ili9341_device_t *dev,
    ili9341_spi_slave_t spi_slave);

void ili9341_spi_write_command(ili9341_device_t *dev,
    ili9341_spi_slave_t spi_slave, uint8_t command);
void ili9341_spi_write_data(ili9341_device_t *dev,
    ili9341_spi_slave_t spi_slave, uint16_t data_sz, uint8_t data[]);
void ili9341_spi_write_data_read(ili9341_device_t *dev,
    ili9341_spi_slave_t spi_slave,
    uint16_t data_sz, uint8_t tx_data[], uint8_t rx_data[]);
void ili9341_spi_write_command_data(ili9341_device_t *dev,
    ili9341_spi_slave_t spi_slave, uint8_t command, uint16_t data_sz, uint8_t data[]);

void ili9341_touch_interrupt(ili9341_device_t *dev);
ili9341_touch_pressed_t ili9341_touch_pressed(ili9341_device_t *dev);

void ili9341_set_touch_pressed_begin(ili9341_device_t *dev,
    ili9341_touch_callback_t callback);
void ili9341_set_touch_pressed_end(ili9341_device_t *dev,
    ili9341_touch_callback_t callback);

ili9341_touch_pressed_t ili9341_touch_coordinate(ili9341_device_t *dev,
    uint16_t *x_pos, uint16_t *y_pos);

#ifdef __cplusplus
}
#endif

#endif /* __ILI9341_H */
