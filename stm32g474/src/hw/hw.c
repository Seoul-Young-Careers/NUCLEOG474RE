/*
 * hw.c
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */


#include "hw.h"


void hwInit(void)
{
  bspInit();

  cliInit();
  ledInit();
  uartInit();
  buttonInit();
  gpioInit();
  pwmInit();

  dm542Init();
}
