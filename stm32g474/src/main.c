/*
 * main.c
 *
 *  Created on: May 9, 2026
 *      Author: young
 */


#include "main.h"

static void mainThread(void *argument);

int main(void)
{
	bspInit();
	osKernelInitialize();

  if (osThreadNew(mainThread, NULL, rtosGetMainThreadAttr()) == NULL)
  {
    ledInit();

    while (1)
    {
      ledToggle(_DEF_LED1);
      delay(50);
    }
  }

  osKernelStart();

  while (1)
  {
  }
}

static void mainThread(void *argument)
{
  UNUSED(argument);

  hwInit();
  apInit();
  apMain();
}
