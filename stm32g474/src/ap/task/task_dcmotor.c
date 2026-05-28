/*
 * task_dcmotor.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/task_dcmotor.h"

bool taskDcMotorInit(void)
{
  return true;
}

bool taskDcMotorRun(uint8_t ch, int16_t speed)
{
#ifdef _USE_BTS7960
  return bts7960Run(ch, speed);
#else
  UNUSED(ch);
  UNUSED(speed);

  return false;
#endif
}

bool taskDcMotorStop(uint8_t ch)
{
#ifdef _USE_BTS7960
  return bts7960Stop(ch);
#else
  UNUSED(ch);

  return false;
#endif
}

bool taskDcMotorBrake(uint8_t ch)
{
#ifdef _USE_BTS7960
  return bts7960Brake(ch);
#else
  UNUSED(ch);

  return false;
#endif
}

bool taskDcMotorCoast(uint8_t ch)
{
#ifdef _USE_BTS7960
  return bts7960Coast(ch);
#else
  UNUSED(ch);

  return false;
#endif
}
