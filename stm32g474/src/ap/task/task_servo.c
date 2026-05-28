/*
 * task_servo.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/task_servo.h"

bool taskServoInit(void)
{
  return true;
}

bool taskServoRun(uint8_t ch, float angle_deg)
{
#ifdef _USE_DS3120MG
  return ds3120mgRun(ch, angle_deg);
#else
  UNUSED(ch);
  UNUSED(angle_deg);

  return false;
#endif
}

bool taskServoCenter(uint8_t ch)
{
#ifdef _USE_DS3120MG
  return ds3120mgCenter(ch);
#else
  UNUSED(ch);

  return false;
#endif
}

bool taskServoStop(uint8_t ch)
{
#ifdef _USE_DS3120MG
  return ds3120mgStop(ch);
#else
  UNUSED(ch);

  return false;
#endif
}
