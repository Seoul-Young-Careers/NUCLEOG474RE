/*
 * task_valve.h
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#ifndef SRC_AP_TASK_TASK_VALVE_H_
#define SRC_AP_TASK_TASK_VALVE_H_

#include "hw.h"

#ifdef __cplusplus
extern "C" {
#endif

bool taskValveInit(void);
bool taskValveOpen(uint8_t ch);
bool taskValveClose(uint8_t ch);
bool taskValveSet(uint8_t ch, bool open);
bool taskValveToggle(uint8_t ch);

#ifdef __cplusplus
}
#endif

#endif /* SRC_AP_TASK_TASK_VALVE_H_ */
