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

// Step motor task, queue, ACK queue를 생성하고 DM542 완료 콜백을 등록한다.
bool taskStepMotorInit(void);

// Home/Start 방향으로 고정 pulse만큼 이동한다. SN04 limit 정책은 task 내부에서 처리한다.
bool taskStepMotorMoveToZero(uint32_t *p_cmd_id);

// End 방향으로 고정 pulse만큼 이동한다. SN04 limit 정책은 task 내부에서 처리한다.
bool taskStepMotorMoveToFull(uint32_t *p_cmd_id);

// SN04_1(Home/Start) 센서를 목표로 이동한다. 센서를 찾지 못하면 ERROR ACK를 반환한다.
bool taskStepMotorMoveToHome(uint32_t *p_cmd_id);

// SN04_2(End) 센서를 목표로 이동한다. 센서를 찾지 못하면 ERROR ACK를 반환한다.
bool taskStepMotorMoveToEnd(uint32_t *p_cmd_id);

// End 센서를 먼저 찾고 Home 센서를 찾아 Home 위치를 0 step 기준으로 보정한다.
bool taskStepMotorCalibration(uint32_t *p_cmd_id);

// 현재 step motor 명령을 중단한다. ACK는 STOP 명령 자체의 완료 여부로 반환된다.
bool taskStepMotorStop(uint32_t *p_cmd_id);

// step motor task가 보낸 ACK를 timeout_ms 동안 기다린다.
bool taskStepMotorGetAck(rtos_step_motor_ack_t *p_ack, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SRC_AP_TASK_TASK_STEPMOTOR_H_ */
