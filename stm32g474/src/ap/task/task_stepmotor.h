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
bool taskStepMotorMoveStep(uint8_t ch, int32_t step, uint32_t pulse_delay_us, uint32_t *p_cmd_id);
bool taskStepMotorMoveToHome(uint32_t *p_cmd_id);
bool taskStepMotorMoveToEnd(uint32_t *p_cmd_id);
bool taskStepMotorStop(uint32_t *p_cmd_id);
bool taskStepMotorGetAck(rtos_step_motor_ack_t *p_ack, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SRC_AP_TASK_TASK_STEPMOTOR_H_ */
