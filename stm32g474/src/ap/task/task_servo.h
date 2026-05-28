/*
 * task_servo.h
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#ifndef SRC_AP_TASK_TASK_SERVO_H_
#define SRC_AP_TASK_TASK_SERVO_H_

#include "hw.h"

#ifdef __cplusplus
extern "C" {
#endif

bool taskServoInit(void);
bool taskServoRun(uint8_t ch, float angle_deg);
bool taskServoCenter(uint8_t ch);
bool taskServoStop(uint8_t ch);

#ifdef __cplusplus
}
#endif

#endif /* SRC_AP_TASK_TASK_SERVO_H_ */
