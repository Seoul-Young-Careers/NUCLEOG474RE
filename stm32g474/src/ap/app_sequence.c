/*
 * app_sequence.c
 *
 *  Created on: May 29, 2026
 *      Author: young
 */

#include "app_sequence.h"

#include "task/app_event.h"
#include "task/task_dcmotor.h"
#include "task/task_servo.h"
#include "task/task_stepmotor.h"
#include "task/task_valve.h"

#define APP_SEQUENCE_CONTROL_EVT    (APP_EVT_RESET_REQ | APP_EVT_STOP_REQ | APP_EVT_START_REQ | APP_EVT_FOOT_PRESS)
#define APP_SEQUENCE_ACK_WAIT_MS    10U
#define APP_SEQUENCE_VALVE_CH       _DEF_2V025_1
#define APP_SEQUENCE_SERVO_CH       _DEF_DS3120MG1
#define APP_SEQUENCE_SERVO_HOME_ANGLE_DEG DS3120MG_ANGLE_DEG
#define APP_SEQUENCE_SERVO_END_ANGLE_DEG 0.0f

typedef enum
{
  APP_SEQUENCE_WAIT_DONE = 0,
  APP_SEQUENCE_WAIT_RESET,
  APP_SEQUENCE_WAIT_STOP,
  APP_SEQUENCE_WAIT_ERROR,
} app_sequence_wait_t;

static app_sequence_state_t app_sequence_state = APP_SEQUENCE_STATE_BOOT;

static bool appSequenceHomeAll(void);
static bool appSequenceMoveToHome(void);
static bool appSequenceMoveToEnd(void);
static bool appSequenceRunEndAction(void);
static bool appSequenceRunFootCycle(void);
static void appSequenceStopAllActuators(void);
static bool appSequenceHandleResetOrStop(void);
static app_sequence_wait_t appSequenceWaitStepMotor(uint32_t cmd_id);
static void appSequenceSetState(app_sequence_state_t state);

bool appSequenceInit(void)
{
  return appSequenceHomeAll();
}

void appSequenceProcess(void)
{
  uint32_t evt;

  evt = appEventWait(APP_SEQUENCE_CONTROL_EVT,
                     osFlagsWaitAny | osFlagsNoClear,
                     osWaitForever);

  if((evt & osFlagsError) != 0U)
  {
    osDelay(10);
    return;
  }

  if((evt & APP_EVT_RESET_REQ) != 0U)
  {
    (void)appEventClear(APP_SEQUENCE_CONTROL_EVT);
    (void)appSequenceHomeAll();
    return;
  }

  if((evt & APP_EVT_STOP_REQ) != 0U)
  {
    (void)appEventClear(APP_EVT_STOP_REQ | APP_EVT_START_REQ | APP_EVT_FOOT_PRESS);
    (void)appSequenceMoveToHome();
    return;
  }

  if((evt & APP_EVT_START_REQ) != 0U)
  {
    (void)appEventClear(APP_EVT_START_REQ);
    (void)appSequenceMoveToEnd();
    return;
  }

  if((evt & APP_EVT_FOOT_PRESS) != 0U)
  {
    (void)appEventClear(APP_EVT_FOOT_PRESS);
    (void)appSequenceRunFootCycle();
    return;
  }
}

app_sequence_state_t appSequenceGetState(void)
{
  return app_sequence_state;
}

static bool appSequenceHomeAll(void)
{
  app_sequence_wait_t wait_result;
  uint32_t cmd_id;

  appSequenceSetState(APP_SEQUENCE_STATE_HOMING);

  (void)appEventClear(APP_SEQUENCE_CONTROL_EVT);
  appSequenceStopAllActuators();

  if(taskStepMotorMoveToHome(&cmd_id) != true)
  {
    appSequenceSetState(APP_SEQUENCE_STATE_ERROR);
    return false;
  }

  wait_result = appSequenceWaitStepMotor(cmd_id);
  if(wait_result == APP_SEQUENCE_WAIT_DONE)
  {
    appSequenceSetState(APP_SEQUENCE_STATE_IDLE_HOME);
    return true;
  }

  if(wait_result == APP_SEQUENCE_WAIT_RESET)
  {
    return appSequenceHomeAll();
  }

  appSequenceSetState(APP_SEQUENCE_STATE_ERROR);

  return false;
}

static bool appSequenceMoveToHome(void)
{
  app_sequence_wait_t wait_result;
  uint32_t cmd_id;

  appSequenceSetState(APP_SEQUENCE_STATE_MOVING_TO_HOME);

  if(taskStepMotorMoveToHome(&cmd_id) != true)
  {
    appSequenceSetState(APP_SEQUENCE_STATE_ERROR);
    return false;
  }

  wait_result = appSequenceWaitStepMotor(cmd_id);
  if(wait_result == APP_SEQUENCE_WAIT_DONE)
  {
    appSequenceSetState(APP_SEQUENCE_STATE_IDLE_HOME);
    return true;
  }

  if(wait_result == APP_SEQUENCE_WAIT_RESET)
  {
    return appSequenceHomeAll();
  }

  appSequenceSetState(APP_SEQUENCE_STATE_ERROR);

  return false;
}

static bool appSequenceMoveToEnd(void)
{
  app_sequence_wait_t wait_result;
  uint32_t cmd_id;

  appSequenceSetState(APP_SEQUENCE_STATE_MOVING_TO_END);

  if(taskStepMotorMoveToEnd(&cmd_id) != true)
  {
    appSequenceSetState(APP_SEQUENCE_STATE_ERROR);
    return false;
  }

  wait_result = appSequenceWaitStepMotor(cmd_id);
  if(wait_result == APP_SEQUENCE_WAIT_DONE)
  {
    if(appSequenceRunEndAction() != true)
    {
      return false;
    }

    appSequenceSetState(APP_SEQUENCE_STATE_READY_SEQUENCE);
    return true;
  }

  if(wait_result == APP_SEQUENCE_WAIT_RESET)
  {
    return appSequenceHomeAll();
  }

  if(wait_result == APP_SEQUENCE_WAIT_STOP)
  {
    (void)appEventClear(APP_EVT_STOP_REQ);
    return appSequenceMoveToHome();
  }

  appSequenceSetState(APP_SEQUENCE_STATE_ERROR);

  return false;
}

static bool appSequenceRunEndAction(void)
{
  appSequenceSetState(APP_SEQUENCE_STATE_END_ACTION);

  if(appSequenceHandleResetOrStop() == true)
  {
    return false;
  }

  if(taskValveOpen(APP_SEQUENCE_VALVE_CH) != true)
  {
    appSequenceSetState(APP_SEQUENCE_STATE_ERROR);
    return false;
  }

  if(appSequenceHandleResetOrStop() == true)
  {
    return false;
  }

  if(taskServoRun(APP_SEQUENCE_SERVO_CH, APP_SEQUENCE_SERVO_END_ANGLE_DEG) != true)
  {
    appSequenceSetState(APP_SEQUENCE_STATE_ERROR);
    return false;
  }

  return true;
}

static bool appSequenceRunFootCycle(void)
{
  if(app_sequence_state != APP_SEQUENCE_STATE_READY_SEQUENCE)
  {
    logPrintf("sequence foot \t\t: ignored\r\n");
    return false;
  }

  appSequenceSetState(APP_SEQUENCE_STATE_RUNNING_SEQUENCE);

  /*
   * FOOT sequence will be filled in after the detailed machine motion is fixed.
   * Keep this function short and interruptible when adding servo, valve, and DC motor actions.
   */
  logPrintf("sequence foot \t\t: TODO\r\n");

  appSequenceSetState(APP_SEQUENCE_STATE_READY_SEQUENCE);

  return true;
}

static void appSequenceStopAllActuators(void)
{
  (void)taskStepMotorStop(NULL);

#ifdef _USE_BTS7960
  for(uint8_t i = 0; i < BTS7960_MAX_CH; i++)
  {
    (void)taskDcMotorStop(i);
  }
#endif

#ifdef _USE_DS3120MG
  for(uint8_t i = 0; i < DS3120MG_MAX_CH; i++)
  {
    (void)taskServoRun(i, APP_SEQUENCE_SERVO_HOME_ANGLE_DEG);
  }
#endif

#ifdef _USE_2V025
  for(uint8_t i = 0; i < V025_MAX_CH; i++)
  {
    (void)taskValveClose(i);
  }
#endif
}

static bool appSequenceHandleResetOrStop(void)
{
  uint32_t evt;

  evt = appEventGet();

  if((evt & APP_EVT_RESET_REQ) != 0U)
  {
    (void)appSequenceHomeAll();
    return true;
  }

  if((evt & APP_EVT_STOP_REQ) != 0U)
  {
    (void)appEventClear(APP_EVT_STOP_REQ);
    (void)appSequenceMoveToHome();
    return true;
  }

  return false;
}

static app_sequence_wait_t appSequenceWaitStepMotor(uint32_t cmd_id)
{
  while(1)
  {
    rtos_step_motor_ack_t ack;
    uint32_t evt;

    evt = appEventGet();

    if((evt & APP_EVT_RESET_REQ) != 0U)
    {
      return APP_SEQUENCE_WAIT_RESET;
    }

    if((evt & APP_EVT_STOP_REQ) != 0U)
    {
      return APP_SEQUENCE_WAIT_STOP;
    }

    if(taskStepMotorGetAck(&ack, APP_SEQUENCE_ACK_WAIT_MS) != true)
    {
      continue;
    }

    if(ack.cmd_id != cmd_id)
    {
      continue;
    }

    switch(ack.result)
    {
      case RTOS_STEP_MOTOR_ACK_DONE:
        return APP_SEQUENCE_WAIT_DONE;

      case RTOS_STEP_MOTOR_ACK_STOPPED:
        return APP_SEQUENCE_WAIT_STOP;

      case RTOS_STEP_MOTOR_ACK_ERROR:
      default:
        return APP_SEQUENCE_WAIT_ERROR;
    }
  }
}

static void appSequenceSetState(app_sequence_state_t state)
{
  app_sequence_state = state;
  logPrintf("sequence state \t\t: %d\r\n", state);
}
