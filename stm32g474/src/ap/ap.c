/*
 * ap.c
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */


#include "ap.h"

static void threadLed(void const *argument);


void apInit(void)
{
  cliOpen(_DEF_UART1, 57600);

}

void apMain(void)
{

  while(1)
  {
    cliMain();
    delay(1);
  }
}

static void threadLed(void const *argument)
{
  UNUSED(argument);


  while(1)
  {
    ledToggle(_DEF_LED1);
    delay(500);
  }
}
