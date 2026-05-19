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

typedef enum
{
  RTOS_MOTOR_CMD_NONE = 0,
  RTOS_MOTOR_CMD_MOVE_STEP,
} rtos_motor_cmd_t;

typedef struct
{
  rtos_motor_cmd_t cmd;
  uint8_t ch;
  int32_t step;
  uint32_t pulse_delay_us;
} rtos_motor_msg_t;

bool rtosInit(void);

const osThreadAttr_t *rtosGetMainThreadAttr(void);
const osThreadAttr_t *rtosGetLedThreadAttr(void);
const osThreadAttr_t *rtosGetMotorThreadAttr(void);
const osMessageQueueAttr_t *rtosGetMotorMsgQAttr(void);

#endif /* SRC_BSP_RTOS_H_ */
