/*
 * app_event.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/app_event.h"

static osEventFlagsId_t app_evt = NULL;

bool appEventInit(void)
{
  if(app_evt != NULL)
  {
    return true;
  }

  app_evt = osEventFlagsNew(rtosGetAppEventAttr());

  return app_evt != NULL;
}

bool appEventIsInit(void)
{
  return app_evt != NULL;
}

uint32_t appEventSet(uint32_t flags)
{
  if(app_evt == NULL) return (uint32_t)osErrorResource;

  return osEventFlagsSet(app_evt, flags);
}

uint32_t appEventClear(uint32_t flags)
{
  if(app_evt == NULL) return (uint32_t)osErrorResource;

  return osEventFlagsClear(app_evt, flags);
}

uint32_t appEventGet(void)
{
  if(app_evt == NULL) return 0U;

  return osEventFlagsGet(app_evt);
}

uint32_t appEventWait(uint32_t flags, uint32_t options, uint32_t timeout_ms)
{
  if(app_evt == NULL) return (uint32_t)osErrorResource;

  return osEventFlagsWait(app_evt, flags, options, timeout_ms);
}
