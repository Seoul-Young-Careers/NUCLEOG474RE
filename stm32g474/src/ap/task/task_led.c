/*
 * task_led.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/task_led.h"

static void threadLed(void *argument);

bool taskLedInit(void)
{
  return osThreadNew(threadLed, NULL, rtosGetLedThreadAttr()) != NULL;
}

static void threadLed(void *argument)
{
  UNUSED(argument);

  while(1)
  {
    ledToggle(_DEF_LED1);
    delay(500);
  }
}
