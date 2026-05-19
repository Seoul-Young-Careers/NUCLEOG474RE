/*
 * main.c
 *
 *  Created on: May 9, 2026
 *      Author: young
 */


#include "main.h"

static void mainThread(void *argument);

static const osThreadAttr_t mainThread_attributes =
{
  .name       = "mainThread",
  .priority   = _HW_DEF_RTOS_THREAD_PRI_MAIN,
  .stack_size = _HW_DEF_RTOS_THREAD_MEM_MAIN,
};

int main(void)
{
	bspInit();

  osKernelInitialize();

  if (osThreadNew(mainThread, NULL, &mainThread_attributes) == NULL)
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
