/*
 * task_dcmotor.h
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#ifndef SRC_AP_TASK_TASK_DCMOTOR_H_
#define SRC_AP_TASK_TASK_DCMOTOR_H_

#include "hw.h"

#ifdef __cplusplus
extern "C" {
#endif

bool taskDcMotorInit(void);
bool taskDcMotorRun(uint8_t ch, int16_t speed);
bool taskDcMotorStop(uint8_t ch);
bool taskDcMotorBrake(uint8_t ch);
bool taskDcMotorCoast(uint8_t ch);

#ifdef __cplusplus
}
#endif

#endif /* SRC_AP_TASK_TASK_DCMOTOR_H_ */
