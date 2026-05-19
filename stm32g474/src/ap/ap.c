/*
 * ap.c
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */


#include "ap.h"

static void threadLed(void *argument);
static void threadMotor(void *argument);

void apInit(void)
{
  cliOpen(_DEF_UART1, 57600);

  if (osThreadNew(threadLed, NULL, rtosGetLedThreadAttr()) != NULL)
  {
    logPrintf("threadLed \t\t: OK\r\n");
  }
  else
  {
    logPrintf("threadLed \t\t: Fail\r\n");

    while (1)
    {
      delay(100);
    }
  }

  if (osThreadNew(threadMotor, NULL, rtosGetMotorThreadAttr()) != NULL)
  {
    logPrintf("threadMotor \t\t: OK\r\n");
  }
  else
  {
    logPrintf("threadMotor \t\t: Fail\r\n");

    while (1)
    {
      delay(100);
    }
  }

}

void apMain(void)
{

  while(1)
  {
    cliMain();
    delay(1);
  }
}

static void threadLed(void *argument)
{
  UNUSED(argument);

  while (1)
  {
    ledToggle(_DEF_LED1);
    osDelay(500);
  }
}

static void threadMotor(void *argument)
{
  UNUSED(argument);

  while (1)
  {
    osDelay(1);
  }
}
