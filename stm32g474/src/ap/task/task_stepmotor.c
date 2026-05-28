/*
 * task_stepmotor.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/task_stepmotor.h"
#include "task/app_event.h"

#ifdef _USE_HW_CLI
static void cliStepMotor(cli_args_t *args);
#endif

#define STEP_MOTOR_RUN_CH       _DEF_DM542_1
#define STEP_MOTOR_RUN_STEP     1
#define STEP_MOTOR_RUN_DELAY_US 1000U
#define STEP_MOTOR_IDLE_MS      1U
#define STEP_MOTOR_LIMIT_EVT    (APP_EVT_SN04_1_DETECTED | APP_EVT_SN04_2_DETECTED)
#define STEP_MOTOR_REQ_STOP_EVT (APP_EVT_RESET_REQ | APP_EVT_STOP_REQ)
#define STEP_MOTOR_STOP_EVT     (STEP_MOTOR_REQ_STOP_EVT | STEP_MOTOR_LIMIT_EVT)

#define STEP_MOTOR_HOMING_DIR        (-1)
#define STEP_MOTOR_HOMING_DELAY_US   1000U
#define STEP_MOTOR_HOMING_MAX_STEPS  100000U

static osMessageQueueId_t step_motor_msg_q = NULL;

static void threadStepMotor(void *argument);
static void taskStepMotorHoming(void);

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

#ifdef _USE_HW_CLI
  cliAdd("stepmotor", cliStepMotor);
#endif

  if(osThreadNew(threadStepMotor, NULL, rtosGetStepMotorThreadAttr()) == NULL)
  {
    logPrintf("threadStepMotor \t: Fail\r\n");
    return false;
  }

  logPrintf("threadStepMotor \t: OK\r\n");

  taskStepMotorHoming();

  return true;
}

bool apStepMotorMoveStep(uint8_t ch, int32_t step, uint32_t pulse_delay_us)
{
  rtos_step_motor_msg_t msg;

  if(step_motor_msg_q == NULL) return false;
  if(pulse_delay_us == 0U) return false;

  msg.cmd            = RTOS_STEP_MOTOR_CMD_MOVE_STEP;
  msg.ch             = ch;
  msg.step           = step;
  msg.pulse_delay_us = pulse_delay_us;

  return osMessageQueuePut(step_motor_msg_q, &msg, 0U, 10U) == osOK;
}

static void taskStepMotorHoming(void)
{
#ifdef _USE_SN04
  if(sn04IsDetected(_DEF_SN04_1) != true)
  {
    int32_t homing_step;

    logPrintf("homing \t\t: start\r\n");

    (void)appEventClear(STEP_MOTOR_LIMIT_EVT);

    homing_step = (int32_t)STEP_MOTOR_HOMING_MAX_STEPS * STEP_MOTOR_HOMING_DIR;

    if(apStepMotorMoveStep(STEP_MOTOR_RUN_CH,
                           homing_step,
                           STEP_MOTOR_HOMING_DELAY_US) == true)
    {
      uint32_t evt;

      evt = appEventWait(APP_EVT_SN04_1_DETECTED,
                         osFlagsWaitAny | osFlagsNoClear,
                         osWaitForever);

      if((evt & osFlagsError) == 0U)
      {
        logPrintf("homing \t\t: done\r\n");
      }
      else
      {
        logPrintf("homing \t\t: wait fail (0x%lx)\r\n", (unsigned long)evt);
      }
    }
    else
    {
      logPrintf("homing \t\t: queue put fail\r\n");
    }

    (void)appEventSet(APP_EVT_STOP_REQ);
  }
  else
  {
    logPrintf("homing \t\t: skip (already at SN04_1)\r\n");
  }
#endif
}

static void threadStepMotor(void *argument)
{
  rtos_step_motor_msg_t msg;
  bool is_button_run = false;
  uint8_t move_ch = 0U;
  int32_t move_remain_step = 0;
  uint32_t move_pulse_delay_us = 0U;

  UNUSED(argument);

  while(1)
  {
    uint32_t evt_flags;

    evt_flags = appEventGet();

    if((evt_flags & STEP_MOTOR_STOP_EVT) != 0U)
    {
      is_button_run = false;
      move_remain_step = 0;
      (void)dm542Stop(STEP_MOTOR_RUN_CH);
      (void)appEventClear(APP_EVT_START_REQ | STEP_MOTOR_REQ_STOP_EVT);
    }
    else if((evt_flags & APP_EVT_START_REQ) != 0U)
    {
      is_button_run = true;
      move_remain_step = 0;
      (void)appEventClear(APP_EVT_START_REQ);
    }

    if(osMessageQueueGet(step_motor_msg_q, &msg, NULL, 0U) == osOK)
    {
      switch(msg.cmd)
      {
        case RTOS_STEP_MOTOR_CMD_MOVE_STEP:
          is_button_run = false;
          move_ch = msg.ch;
          move_remain_step = msg.step;
          move_pulse_delay_us = msg.pulse_delay_us;
          break;

        default:
          break;
      }
    }

    evt_flags = appEventGet();
    if((evt_flags & STEP_MOTOR_STOP_EVT) != 0U)
    {
      is_button_run = false;
      move_remain_step = 0;
      (void)dm542Stop(STEP_MOTOR_RUN_CH);
      (void)appEventClear(STEP_MOTOR_REQ_STOP_EVT);
      osDelay(STEP_MOTOR_IDLE_MS);
      continue;
    }

    if(move_remain_step != 0)
    {
      int32_t step = (move_remain_step > 0) ? 1 : -1;

      if(dm542MoveStep(move_ch, step, move_pulse_delay_us) == true)
      {
        move_remain_step -= step;
      }
      else
      {
        move_remain_step = 0;
      }
    }
    else if(is_button_run == true)
    {
      if(dm542MoveStep(STEP_MOTOR_RUN_CH, STEP_MOTOR_RUN_STEP, STEP_MOTOR_RUN_DELAY_US) != true)
      {
        is_button_run = false;
      }
    }
    else
    {
      osDelay(STEP_MOTOR_IDLE_MS);
    }
  }
}

#ifdef _USE_HW_CLI
static void cliStepMotor(cli_args_t *args)
{
  bool ret = false;
  bool cmd_ret;
  uint8_t ch;
  int32_t step;
  uint32_t value;

  if(args->argc == 1)
  {
    if(args->isStr(0, "show") == true)
    {
      cliPrintf("stepmotor queue count:%lu space:%lu\n",
                (unsigned long)osMessageQueueGetCount(step_motor_msg_q),
                (unsigned long)osMessageQueueGetSpace(step_motor_msg_q));

      for(uint8_t i = 0; i < DM542_MAX_CH; i++)
      {
        cliPrintf("stepmotor %d open:%d busy:%d\n",
                  i,
                  dm542IsOpen(i),
                  dm542IsBusy(i));
      }

      ret = true;
    }
  }

  if(args->argc == 3)
  {
    ch   = (uint8_t)args->getData(1);
    step = args->getData(2);

    if(args->isStr(0, "run") == true)
    {
      cmd_ret = apStepMotorMoveStep(ch, step, 1000U);
      cliPrintf("stepmotor run %d %ld : %s\n",
                ch,
                (long)step,
                cmd_ret ? "QUEUED" : "FAIL");
      ret = true;
    }
  }

  if(args->argc == 4)
  {
    ch    = (uint8_t)args->getData(1);
    step  = args->getData(2);
    value = (uint32_t)args->getData(3);

    if(args->isStr(0, "move") == true)
    {
      cmd_ret = apStepMotorMoveStep(ch, step, value);
      cliPrintf("stepmotor move %d %ld %luus : %s\n",
                ch,
                (long)step,
                value,
                cmd_ret ? "QUEUED" : "FAIL");
      ret = true;
    }
  }

  if(ret != true)
  {
    cliPrintf("stepmotor show\n");
    cliPrintf("stepmotor run  ch[0~%d] step\n", DM542_MAX_CH - 1);
    cliPrintf("stepmotor move ch[0~%d] step pulse_delay_us\n", DM542_MAX_CH - 1);
  }
}
#endif
