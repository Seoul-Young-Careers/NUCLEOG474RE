/*
 * task_pump.c
 *
 *  Created on: Jun 1, 2026
 *      Author: young
 */

#include "task/task_pump.h"

bool taskPumpInit(void)
{
  return true;
}

bool taskPumpOn(void)
{
#ifdef _USE_PUMP
  return pumpOn();
#else
  return false;
#endif
}

bool taskPumpOff(void)
{
#ifdef _USE_PUMP
  return pumpOff();
#else
  return false;
#endif
}

bool taskPumpSet(bool on)
{
#ifdef _USE_PUMP
  return pumpSet(on);
#else
  UNUSED(on);

  return false;
#endif
}

bool taskPumpToggle(void)
{
#ifdef _USE_PUMP
  return pumpToggle();
#else
  return false;
#endif
}

bool taskPumpIsOn(void)
{
#ifdef _USE_PUMP
  return pumpIsOn();
#else
  return false;
#endif
}
