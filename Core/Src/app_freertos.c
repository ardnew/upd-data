/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "message_buffer.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

osThreadId screenTaskHandle;
osSemaphoreId screenLockHandle;

/* USER CODE END Variables */
osThreadId defaultTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void ScreenTask(void const * argument);
void testScreenFill(ili9341_device_t *dev);
void testScreenLines(ili9341_device_t *dev);
void testScreenRects(ili9341_device_t *dev);
void testScreenCircles(ili9341_device_t *dev);
void testScreenText(ili9341_device_t *dev);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
//  screenMessage = xMessageBufferCreate(screenMessageSize);
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  osSemaphoreDef(screenLock);
  screenLockHandle = osSemaphoreCreate(osSemaphore(screenLock), 1);
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  osThreadDef(screenTask, ScreenTask, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
  screenTaskHandle = osThreadCreate(osThread(screenTask), NULL);
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void ScreenTask(void const * argument)
{
  for (;;)
  {
    if (NULL != screenLockHandle)
    {
      if (osOK == osSemaphoreWait(screenLockHandle, osWaitForever))
      {
        ili9341_device_t *dev = screen();

        testScreenFill(dev);
        testScreenLines(dev);
        osDelay(1000);
        testScreenRects(dev);
        osDelay(1000);
        testScreenCircles(dev);
        osDelay(1000);
        //testScreenText(dev);
        //osDelay(1000);

        osSemaphoreRelease(screenLockHandle);
      }
    }
  }

//  uint8_t message[screenMessageSize];
//  char text[screenMessageSize - 3];
//  uint8_t textSize;
//  size_t recvCount;
//
//  ili9341_text_attr_t textAttr = (ili9341_text_attr_t){
//    .font = &ili9341_font_7x10,
//    .fg_color = ILI9341_WHITE,
//    .bg_color = ILI9341_BLUE,
//    .origin_x = 10,
//    .origin_y = 10
//  };
  for (;;)
  {
//    recvCount =
//        xMessageBufferReceive(screenMessage, (void *)message, sizeof(message), portMAX_DELAY);
//
//    if (recvCount > 2)
//    {
//      textAttr.origin_x = message[0];
//      textAttr.origin_y = message[1];
//      textSize          = message[2];
//
//      if (NULL != screenLockHandle)
//      {
//        if (osOK == osSemaphoreWait(screenLockHandle, osWaitForever))
//        {
//          memcpy(text, message + 2, textSize);
//          ili9341_fill_rect(screen(), textAttr.bg_color,
//              textAttr.origin_x, textAttr.origin_y, screen()->screen_size.width / 2, textAttr.font->height);
//          ili9341_draw_string(screen(), textAttr, text);
//          osSemaphoreRelease(screenLockHandle);
//        }
//      }
//    }
  }
}

void testScreenFill(ili9341_device_t *dev)
{
  ili9341_fill_screen(dev, ILI9341_BLUE);
  osDelay(500);
  ili9341_fill_screen(dev, ILI9341_RED);
  osDelay(500);
  ili9341_fill_screen(dev, ILI9341_GREEN);
  osDelay(500);
  ili9341_draw_bitmap_1b(dev, ILI9341_DARKGREY, ILI9341_LIGHTGREY, 0, 0, 320, 240, NULL);
  osDelay(2000);
}

void testScreenLines(ili9341_device_t *dev)
{
  ili9341_fill_screen(dev, ILI9341_BLACK);

  ili9341_color_t color;
  uint8_t wheel = 0U;

  for (int32_t i = 0; i < dev->screen_size.height; i += 10)
    { color = ili9341_color_wheel(&wheel); wheel += 3;
      ili9341_draw_line(dev, color, 0, 0, dev->screen_size.width, i); }
  for (int32_t i = dev->screen_size.width; i >= 0; i -= 10)
    { color = ili9341_color_wheel(&wheel); wheel += 3;
      ili9341_draw_line(dev, color, 0, 0, i, dev->screen_size.height); }

  for (int32_t i = 0; i < dev->screen_size.height; i += 10)
    { color = ili9341_color_wheel(&wheel); wheel += 3;
      ili9341_draw_line(dev, color, dev->screen_size.width, dev->screen_size.height, 0, dev->screen_size.height - i); }
  for (int32_t i = dev->screen_size.width; i >= 0; i -= 10)
    { color = ili9341_color_wheel(&wheel); wheel += 3;
      ili9341_draw_line(dev, color, dev->screen_size.width, dev->screen_size.height, dev->screen_size.width - i, 0); }
}

void testScreenRects(ili9341_device_t *dev)
{
  ili9341_fill_screen(dev, ILI9341_BLACK);

  ili9341_color_t color;
  uint8_t wheel = 0U;

  for (uint16_t i = 0, in = 0; i < dev->screen_size.width; i += 16, ++in) {
    for (uint16_t j = 0, jn = 0; j < dev->screen_size.height; j += 16, ++jn) {
      color = ili9341_color_wheel(&wheel);
      if ((in & 1) == (jn & 1)) {
        ili9341_fill_rect(dev, color, i, j, 16, 16);
      } else {
        ili9341_draw_rect(dev, ~color, i+2, j+2, 12, 12);
      }
    }
  }
}

void testScreenCircles(ili9341_device_t *dev)
{
  ili9341_fill_screen(dev, ILI9341_BLACK);

  uint16_t x_mid = dev->screen_size.width / 2;
  uint16_t y_mid = dev->screen_size.height / 2;
  uint16_t radius = x_mid;
  if (radius < y_mid)
    { radius = y_mid; }

  ili9341_color_t color;
  uint8_t wheel = 0U;

  for (int16_t i = radius; i >= 0; i -= 6) {
    color = ili9341_color_wheel(&wheel);
    wheel += 9;
    ili9341_fill_circle(dev, color, x_mid, y_mid, i);
  }

  for (int16_t i = 0, n = 0; i < dev->screen_size.width; i += 24, ++n) {
    for (int16_t j = 0, m = 0; j < dev->screen_size.height; j += 24, ++m) {

      color = ili9341_color_wheel(&wheel);
      wheel += 3;

      if ((n & 1) == (m & 1)) {
        ili9341_draw_circle(dev, color, i + 12, j + 12, 16);
      }
      else {
        ili9341_draw_circle(dev, color, i + 12, j + 12, 8);
      }
    }
  }
}

void testScreenText(ili9341_device_t *dev)
{
  ili9341_fill_screen(dev, ILI9341_BLACK);

  ili9341_text_attr_t textAttr = (ili9341_text_attr_t){
    .font = &ili9341_font_11x18,
    .fg_color = ILI9341_WHITE,
    .bg_color = ILI9341_RED,
    .origin_x = 10,
    .origin_y = 10
  };
  ili9341_draw_string(dev, textAttr, "Hello world!");

}
/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
