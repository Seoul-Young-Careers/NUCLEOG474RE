/*
 * task_pump.h
 *
 *  Created on: Jun 1, 2026
 *      Author: young
 */

#ifndef SRC_AP_TASK_TASK_PUMP_H_
#define SRC_AP_TASK_TASK_PUMP_H_

#include "hw.h"

#ifdef __cplusplus
extern "C" {
#endif

bool taskPumpInit(void);
bool taskPumpOn(void);
bool taskPumpOff(void);
bool taskPumpSet(bool on);
bool taskPumpToggle(void);
bool taskPumpIsOn(void);

#ifdef __cplusplus
}
#endif

#endif /* SRC_AP_TASK_TASK_PUMP_H_ */
