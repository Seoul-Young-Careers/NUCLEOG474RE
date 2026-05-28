/*
 * task_valve.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/task_valve.h"

bool taskValveInit(void)
{
  return true;
}

bool taskValveOpen(uint8_t ch)
{
#ifdef _USE_2V025
  return v025ValveOpen(ch);
#else
  UNUSED(ch);

  return false;
#endif
}

bool taskValveClose(uint8_t ch)
{
#ifdef _USE_2V025
  return v025ValveClose(ch);
#else
  UNUSED(ch);

  return false;
#endif
}

bool taskValveSet(uint8_t ch, bool open)
{
#ifdef _USE_2V025
  return v025ValveSet(ch, open);
#else
  UNUSED(ch);
  UNUSED(open);

  return false;
#endif
}

bool taskValveToggle(uint8_t ch)
{
#ifdef _USE_2V025
  return v025ValveToggle(ch);
#else
  UNUSED(ch);

  return false;
#endif
}
