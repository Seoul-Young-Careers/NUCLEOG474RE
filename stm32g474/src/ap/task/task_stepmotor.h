/*
 * task_stepmotor.h
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#ifndef SRC_AP_TASK_TASK_STEPMOTOR_H_
#define SRC_AP_TASK_TASK_STEPMOTOR_H_

#include "hw.h"

#ifdef __cplusplus
extern "C" {
#endif

bool taskStepMotorInit(void);
bool apStepMotorMoveStep(uint8_t ch, int32_t step, uint32_t pulse_delay_us);

#ifdef __cplusplus
}
#endif

#endif /* SRC_AP_TASK_TASK_STEPMOTOR_H_ */
