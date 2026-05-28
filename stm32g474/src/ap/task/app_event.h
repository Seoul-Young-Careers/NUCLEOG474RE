/*
 * app_event.h
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#ifndef SRC_AP_TASK_APP_EVENT_H_
#define SRC_AP_TASK_APP_EVENT_H_

#include "rtos.h"

#ifdef __cplusplus
extern "C" {
#endif

bool appEventInit(void);
bool appEventIsInit(void);
uint32_t appEventSet(uint32_t flags);
uint32_t appEventClear(uint32_t flags);
uint32_t appEventGet(void);
uint32_t appEventWait(uint32_t flags, uint32_t options, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SRC_AP_TASK_APP_EVENT_H_ */
