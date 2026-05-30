/*
 * task_stepmotor.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/task_stepmotor.h"
#include "task/app_event.h"

#define STEP_MOTOR_IDLE_MS             1U
#define STEP_MOTOR_PULSE_DELAY_US      1000U
#define STEP_MOTOR_TRAVEL_MAX_STEPS    100000

#define STEP_MOTOR_HOME_DIR            (-1)
#define STEP_MOTOR_END_DIR             1

#define STEP_MOTOR_CONTROL_EVT         APP_EVT_RESET_REQ

static osMessageQueueId_t step_motor_msg_q = NULL;
static osMessageQueueId_t step_motor_ack_q = NULL;
static uint32_t step_motor_cmd_id = 0U;

static void threadStepMotor(void *argument);
static uint32_t taskStepMotorNextCmdId(void);
static bool taskStepMotorPutMsg(rtos_step_motor_msg_t *p_msg, uint32_t *p_cmd_id, bool clear_queue);
static void taskStepMotorSendAck(const rtos_step_motor_msg_t *p_msg, rtos_step_motor_ack_result_t result);
static void taskStepMotorStopCurrent(uint8_t ch);

#ifdef _USE_SN04
static bool taskStepMotorIsTargetDetected(uint32_t target_evt);
#endif

bool taskStepMotorInit(void)
{
  step_motor_msg_q = osMessageQueueNew(_HW_DEF_RTOS_MSG_Q_STEP_MOTOR,
                                       sizeof(rtos_step_motor_msg_t),
                                       rtosGetStepMotorMsgQAttr());

  if(step_motor_msg_q == NULL)
  {
    logPrintf("stepMotorMsgQ \t\t: Fail\r\n");
    return false;
  }

  logPrintf("stepMotorMsgQ \t\t: OK\r\n");

  step_motor_ack_q = osMessageQueueNew(_HW_DEF_RTOS_MSG_Q_STEP_MOTOR_ACK,
                                       sizeof(rtos_step_motor_ack_t),
                                       rtosGetStepMotorAckQAttr());

  if(step_motor_ack_q == NULL)
  {
    logPrintf("stepMotorAckQ \t\t: Fail\r\n");
    return false;
  }

  logPrintf("stepMotorAckQ \t\t: OK\r\n");

  if(osThreadNew(threadStepMotor, NULL, rtosGetStepMotorThreadAttr()) == NULL)
  {
    logPrintf("threadStepMotor \t: Fail\r\n");
    return false;
  }

  logPrintf("threadStepMotor \t: OK\r\n");

  return true;
}

bool taskStepMotorMoveStep(uint8_t ch, int32_t step, uint32_t pulse_delay_us, uint32_t *p_cmd_id)
{
  rtos_step_motor_msg_t msg;

  if(pulse_delay_us == 0U) return false;

  msg.cmd            = RTOS_STEP_MOTOR_CMD_MOVE_STEP;
  msg.ch             = ch;
  msg.step           = step;
  msg.pulse_delay_us = pulse_delay_us;

  return taskStepMotorPutMsg(&msg, p_cmd_id, true);
}

bool taskStepMotorMoveToHome(uint32_t *p_cmd_id)
{
  rtos_step_motor_msg_t msg;

  msg.cmd            = RTOS_STEP_MOTOR_CMD_MOVE_TO_HOME;
  msg.ch             = _DEF_DM542_1;
  msg.step           = STEP_MOTOR_HOME_DIR * STEP_MOTOR_TRAVEL_MAX_STEPS;
  msg.pulse_delay_us = STEP_MOTOR_PULSE_DELAY_US;

  return taskStepMotorPutMsg(&msg, p_cmd_id, true);
}

bool taskStepMotorMoveToEnd(uint32_t *p_cmd_id)
{
  rtos_step_motor_msg_t msg;

  msg.cmd            = RTOS_STEP_MOTOR_CMD_MOVE_TO_END;
  msg.ch             = _DEF_DM542_1;
  msg.step           = STEP_MOTOR_END_DIR * STEP_MOTOR_TRAVEL_MAX_STEPS;
  msg.pulse_delay_us = STEP_MOTOR_PULSE_DELAY_US;

  return taskStepMotorPutMsg(&msg, p_cmd_id, true);
}

bool taskStepMotorStop(uint32_t *p_cmd_id)
{
  rtos_step_motor_msg_t msg;

  msg.cmd            = RTOS_STEP_MOTOR_CMD_STOP;
  msg.ch             = _DEF_DM542_1;
  msg.step           = 0;
  msg.pulse_delay_us = STEP_MOTOR_PULSE_DELAY_US;

  return taskStepMotorPutMsg(&msg, p_cmd_id, true);
}

bool taskStepMotorGetAck(rtos_step_motor_ack_t *p_ack, uint32_t timeout_ms)
{
  if(step_motor_ack_q == NULL) return false;
  if(p_ack == NULL) return false;

  return osMessageQueueGet(step_motor_ack_q, p_ack, NULL, timeout_ms) == osOK;
}

static uint32_t taskStepMotorNextCmdId(void)
{
  step_motor_cmd_id++;
  if(step_motor_cmd_id == 0U)
  {
    step_motor_cmd_id++;
  }

  return step_motor_cmd_id;
}

static bool taskStepMotorPutMsg(rtos_step_motor_msg_t *p_msg, uint32_t *p_cmd_id, bool clear_queue)
{
  uint32_t cmd_id;

  if(step_motor_msg_q == NULL) return false;
  if(step_motor_ack_q == NULL) return false;
  if(p_msg == NULL) return false;

  if(clear_queue == true)
  {
    (void)osMessageQueueReset(step_motor_msg_q);
    (void)osMessageQueueReset(step_motor_ack_q);
  }

  cmd_id = taskStepMotorNextCmdId();
  p_msg->cmd_id = cmd_id;

  if(osMessageQueuePut(step_motor_msg_q, p_msg, 0U, 10U) != osOK)
  {
    return false;
  }

  if(p_cmd_id != NULL)
  {
    *p_cmd_id = cmd_id;
  }

  return true;
}

static void threadStepMotor(void *argument)
{
  rtos_step_motor_msg_t msg;
  rtos_step_motor_msg_t active_msg = {0};
  bool has_active_msg = false;
  uint8_t move_ch = _DEF_DM542_1;
  int32_t move_remain_step = 0;
  uint32_t move_pulse_delay_us = STEP_MOTOR_PULSE_DELAY_US;
  uint32_t target_evt = 0U;
  bool is_target_move = false;

  UNUSED(argument);

  while(1)
  {
    uint32_t evt_flags;

    evt_flags = appEventGet();
    if((evt_flags & STEP_MOTOR_CONTROL_EVT) != 0U)
    {
      move_remain_step = 0;
      target_evt = 0U;
      is_target_move = false;
      taskStepMotorStopCurrent(move_ch);

      if(has_active_msg == true)
      {
        taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_STOPPED);
        has_active_msg = false;
      }

      osDelay(STEP_MOTOR_IDLE_MS);
      continue;
    }

    if(osMessageQueueGet(step_motor_msg_q, &msg, NULL, 0U) == osOK)
    {
      if(has_active_msg == true)
      {
        taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_STOPPED);
      }

      active_msg = msg;
      has_active_msg = true;
      move_ch = msg.ch;
      move_pulse_delay_us = msg.pulse_delay_us;
      target_evt = 0U;
      is_target_move = false;

      switch(msg.cmd)
      {
        case RTOS_STEP_MOTOR_CMD_MOVE_STEP:
          move_remain_step = msg.step;
          if(move_remain_step == 0)
          {
            taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
            has_active_msg = false;
          }
          break;

        case RTOS_STEP_MOTOR_CMD_MOVE_TO_HOME:
#ifdef _USE_SN04
          target_evt = APP_EVT_SN04_1_DETECTED;
          is_target_move = true;

          if(taskStepMotorIsTargetDetected(target_evt) == true)
          {
            move_remain_step = 0;
            taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
            has_active_msg = false;
          }
          else
          {
            move_remain_step = msg.step;
          }
#else
          move_remain_step = 0;
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
          has_active_msg = false;
#endif
          break;

        case RTOS_STEP_MOTOR_CMD_MOVE_TO_END:
#ifdef _USE_SN04
          target_evt = APP_EVT_SN04_2_DETECTED;
          is_target_move = true;

          if(taskStepMotorIsTargetDetected(target_evt) == true)
          {
            move_remain_step = 0;
            taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
            has_active_msg = false;
          }
          else
          {
            move_remain_step = msg.step;
          }
#else
          move_remain_step = 0;
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
          has_active_msg = false;
#endif
          break;

        case RTOS_STEP_MOTOR_CMD_STOP:
          move_remain_step = 0;
          taskStepMotorStopCurrent(move_ch);
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
          has_active_msg = false;
          break;

        default:
          move_remain_step = 0;
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
          has_active_msg = false;
          break;
      }
    }

#ifdef _USE_SN04
    if((has_active_msg == true) && (target_evt != 0U) && (taskStepMotorIsTargetDetected(target_evt) == true))
    {
      move_remain_step = 0;
      taskStepMotorStopCurrent(move_ch);
      taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
      has_active_msg = false;
      osDelay(STEP_MOTOR_IDLE_MS);
      continue;
    }
#endif

    if(move_remain_step != 0)
    {
      int32_t step = (move_remain_step > 0) ? 1 : -1;

      if(dm542MoveStep(move_ch, step, move_pulse_delay_us) == true)
      {
        move_remain_step -= step;

        if(move_remain_step == 0)
        {
          if(is_target_move == true)
          {
            taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
          }
          else
          {
            taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
          }

          has_active_msg = false;
        }
      }
      else
      {
        move_remain_step = 0;
        taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
        has_active_msg = false;
      }
    }
    else
    {
      osDelay(STEP_MOTOR_IDLE_MS);
    }
  }
}

static void taskStepMotorSendAck(const rtos_step_motor_msg_t *p_msg, rtos_step_motor_ack_result_t result)
{
  rtos_step_motor_ack_t ack;

  if(step_motor_ack_q == NULL) return;
  if(p_msg == NULL) return;
  if(p_msg->cmd_id == 0U) return;

  ack.cmd_id = p_msg->cmd_id;
  ack.cmd    = p_msg->cmd;
  ack.result = result;

  if(osMessageQueuePut(step_motor_ack_q, &ack, 0U, 0U) != osOK)
  {
    (void)osMessageQueueReset(step_motor_ack_q);
    (void)osMessageQueuePut(step_motor_ack_q, &ack, 0U, 0U);
  }
}

static void taskStepMotorStopCurrent(uint8_t ch)
{
  (void)dm542Stop(ch);
}

#ifdef _USE_SN04
static bool taskStepMotorIsTargetDetected(uint32_t target_evt)
{
  switch(target_evt)
  {
    case APP_EVT_SN04_1_DETECTED:
      return sn04IsDetected(_DEF_SN04_1);

    case APP_EVT_SN04_2_DETECTED:
      return sn04IsDetected(_DEF_SN04_2);

    default:
      return false;
  }
}
#endif
