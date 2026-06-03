/*
 * rtos.h
 *
 *  Created on: May 18, 2026
 *      Author: TEMP
 */

#ifndef SRC_BSP_RTOS_H_
#define SRC_BSP_RTOS_H_

#include "def.h"
#include "rtos_def.h"

#define APP_EVT_RESET_REQ        (1 << 0)
#define APP_EVT_STOP_REQ         (1 << 1)
#define APP_EVT_START_REQ        (1 << 2)
#define APP_EVT_FOOT_PRESS       (1 << 3)
#define APP_EVT_SN04_1_DETECTED  (1 << 4)
#define APP_EVT_SN04_2_DETECTED  (1 << 5)
// DM542 비동기 STEP 출력이 지정 count만큼 끝났음을 알리는 이벤트
#define APP_EVT_STEP_MOTOR_DONE  (1 << 6)

typedef enum
{
  RTOS_STEP_MOTOR_NONE = 0,
  RTOS_STEP_MOTOR_MOVE_TO_ZERO,
  RTOS_STEP_MOTOR_MOVE_TO_FULL,
  RTOS_STEP_MOTOR_MOVE_TO_HOME,
  RTOS_STEP_MOTOR_MOVE_TO_END,
  RTOS_STEP_MOTOR_CALIBRATION,
  RTOS_STEP_MOTOR_STOP,
} rtos_step_motor_cmd_t;

typedef enum
{
  RTOS_STEP_MOTOR_ACK_DONE = 0,
  RTOS_STEP_MOTOR_ACK_STOPPED,
  RTOS_STEP_MOTOR_ACK_ERROR,
} rtos_step_motor_ack_result_t;

typedef struct
{
  uint32_t cmd_id;
  rtos_step_motor_cmd_t cmd;
  uint8_t ch;
  int32_t step;
  uint32_t pulse_delay_us;
} rtos_step_motor_msg_t;

typedef struct
{
  uint32_t cmd_id;
  rtos_step_motor_cmd_t cmd;
  rtos_step_motor_ack_result_t result;
} rtos_step_motor_ack_t;

bool rtosInit(void);

const osThreadAttr_t *rtosGetMainThreadAttr(void);
const osThreadAttr_t *rtosGetLedThreadAttr(void);

const osThreadAttr_t *rtosGetStepMotorThreadAttr(void);
const osMessageQueueAttr_t *rtosGetStepMotorMsgQAttr(void);
const osMessageQueueAttr_t *rtosGetStepMotorAckQAttr(void);

const osThreadAttr_t *rtosGetButtonThreadAttr(void);
const osThreadAttr_t *rtosGetSensorThreadAttr(void);
const osThreadAttr_t *rtosGetSequenceThreadAttr(void);
const osEventFlagsAttr_t *rtosGetAppEventAttr(void);


#endif /* SRC_BSP_RTOS_H_ */
