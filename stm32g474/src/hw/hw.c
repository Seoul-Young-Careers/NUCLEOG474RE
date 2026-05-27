/*
 * hw.c
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */


#include "hw.h"


void hwInit(void)
{
  cliInit();
  ledInit();
  uartInit();
  buttonInit();
  gpioInit();
  pwmInit();

  dm542Init();
#ifdef _USE_BTS7960
  bts7960Init();
#endif
  ds3120mgInit();
  sn04Init();
  v025Init();
}
