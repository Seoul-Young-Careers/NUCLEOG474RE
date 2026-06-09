/*
 * app_sequence.c
 *
 *  Created on: May 29, 2026
 *      Author: young
 */

#include "app_sequence.h"

#include "task/app_event.h"
#include "task/task_dcmotor.h"
#include "task/task_pump.h"
#include "task/task_servo.h"
#include "task/task_stepmotor.h"
#include "task/task_valve.h"

#define CONTROL_EVT                 (APP_EVT_RESET_REQ | APP_EVT_STOP_REQ | APP_EVT_START_REQ | APP_EVT_FOOT_PRESS)
#define ACK_WAIT_MS                 10

#define SERVO_CH                              _DEF_DS3120MG1
#define VALVE1_CH                             _DEF_2V025_1
#define VALVE2_CH                             _DEF_2V025_2

#define SERVO_HOME_ANGLE_DEG        			    180.0f    
#define SERVO_START_GRAB_BAG_ANGLE_DEG		    	20.0f
#define SERVO_START_PUT_BAG_ANGLE_DEG         		60.0f
#define SERVO_START_OPEN_BAG_ANGLE_DEG		    	170.0f
#define SERVO_HOLD_ANGLE_DEG        			    120.0f

#define SERVO_WAIT_MS               				  500
#define SERVO_PUT_SETTLE_DELAY_MS      		    300

typedef enum
{
  APP_SEQUENCE_WAIT_DONE = 0,
  APP_SEQUENCE_WAIT_RESET,
  APP_SEQUENCE_WAIT_STOP,
  APP_SEQUENCE_WAIT_ERROR,
} app_sequence_wait_t;

static app_sequence_state_t app_sequence_state = APP_SEQUENCE_STATE_BOOT;

/*

 */
static bool runResetSequence(void);															// RESET 버튼 또는 전원 초기화 시 HOME 기준을 잡고 시작 위치로 이동시킨다.

static bool runStopSequence(void); 															// STOP 버튼을 눌렀을 때 밸브/서보를 초기화하고 스텝모터를 HOME으로 이동시킨다.

static bool runStartSequence(void);															// START 버튼을 눌렀을 때 필요한 전체 시작 시퀀스를 실행한다.

static bool runFootSwitchSequence(void);												// FOOT 스위치를 눌렀을 때 반복 장비 시퀀스를 처리한다.

static bool isStopSequenceAllowed(void);												// START 이후 상태에서만 STOP 시퀀스를 실행할 수 있는지 확인한다.
static void enterErrorState(void);														// 에러 발생 시 모든 구동부를 안전 상태로 내리고 ERROR 상태로 고정한다.
static void stopAllActuators(void);															// reset/error 시 모든 구동부를 안전한 기본 상태로 되돌린다.
static bool handleResetStopRequest(void);												// 긴 동작 중 RESET 또는 STOP 요청이 들어왔는지 확인하고 우선 처리한다.
static bool servoMoveAndWait(float angle_deg);									// 서보를 지정 각도로 이동시키고, 이동 시간 동안 reset/stop을 감시한다.

static bool delayInterruptible(uint32_t delay_ms);							// 긴 대기 시간을 짧게 쪼개 reset/stop 요청에 반응할 수 있게 한다.

static app_sequence_wait_t waitStepMotor(uint32_t cmd_id);			// 스텝모터 명령 ACK를 기다리면서 reset/stop 요청을 함께 감시한다.
static void setState(app_sequence_state_t state);								// 현재 시퀀스 상태를 갱신하고 로그로 남긴다.

// 부팅 시 장비 위치를 보정한 뒤 서보를 부팅 전용 초기 각도로 보낸다.
bool sequenceInit(void)
{
	servoMoveAndWait(SERVO_HOME_ANGLE_DEG);

  return runResetSequence();
}

// 버튼/센서 이벤트를 기다렸다가 reset, stop, start, foot 요청을 분기 처리한다.
void sequenceProcess(void)
{
  uint32_t evt;

  evt = appEventWait(CONTROL_EVT,
                     osFlagsWaitAny | osFlagsNoClear,
                     osWaitForever);

  if((evt & osFlagsError) != 0U)
  {
    osDelay(10);
    return;
  }

  if((evt & APP_EVT_RESET_REQ) != 0U)
  {
    (void)appEventClear(CONTROL_EVT);
    (void)runResetSequence();
    return;
  }

  if((evt & APP_EVT_STOP_REQ) != 0U)
  {
    if(isStopSequenceAllowed() == true)
    {
      (void)appEventClear(APP_EVT_STOP_REQ | APP_EVT_START_REQ | APP_EVT_FOOT_PRESS);
      (void)runStopSequence();
    }
    else
    {
      (void)appEventClear(APP_EVT_STOP_REQ);
    }
    return;
  }

  if((evt & APP_EVT_START_REQ) != 0U)
  {
    if(app_sequence_state == APP_SEQUENCE_STATE_IDLE_HOME)
    {
      (void)appEventClear(APP_EVT_START_REQ | APP_EVT_FOOT_PRESS);
      (void)runStartSequence();
    }
    else
    {
      (void)appEventClear(APP_EVT_START_REQ);
    }
    return;
  }

  if((evt & APP_EVT_FOOT_PRESS) != 0U)
  {
    if(app_sequence_state == APP_SEQUENCE_STATE_READY_SEQUENCE)
    {
      (void)appEventClear(APP_EVT_FOOT_PRESS | APP_EVT_START_REQ);
      (void)runFootSwitchSequence();
    }
    else
    {
      (void)appEventClear(APP_EVT_FOOT_PRESS);
    }
    return;
  }
}

// 외부에서 현재 시퀀스 상태를 확인할 수 있게 반환한다.
app_sequence_state_t sequenceGetState(void)
{
  return app_sequence_state;
}

// RESET 버튼 또는 전원 초기화 시 HOME 기준을 잡고 시작 위치로 이동시킨다.
static bool runResetSequence(void)
{
  app_sequence_wait_t wait_result;
  uint32_t cmd_id;

  setState(APP_SEQUENCE_STATE_HOMING);

  (void)appEventClear(CONTROL_EVT);
  stopAllActuators();

  // RESET 시에는 End 센서까지 갔다가 Home 센서까지 돌아오는 Calibration 명령 하나만 수행한다.
  if(taskStepMotorCalibration(&cmd_id) != true)
  {
    enterErrorState();
    return false;
  }

  wait_result = waitStepMotor(cmd_id);

  if(wait_result != APP_SEQUENCE_WAIT_DONE)
  {
    enterErrorState();
    return false;
  }

  (void)appEventClear(CONTROL_EVT);
  setState(APP_SEQUENCE_STATE_IDLE_HOME);

  return true;
}

// STOP 버튼을 눌렀을 때 밸브를 닫고 서보를 HOME 각도로 보낸 뒤 스텝모터를 HOME으로 이동시킨다.
static bool runStopSequence(void)
{
  app_sequence_wait_t wait_result;
  uint32_t cmd_id;

  setState(APP_SEQUENCE_STATE_MOVING_TO_HOME);

  (void)taskValveClose(VALVE1_CH);
  (void)taskValveClose(VALVE2_CH);
  (void)taskPumpOff();

  if(servoMoveAndWait(SERVO_HOME_ANGLE_DEG) != true)
  {
    return false;
  }

  if(taskStepMotorMoveToHome(&cmd_id) != true)
  {
    enterErrorState();
    return false;
  }

  wait_result = waitStepMotor(cmd_id);
  if(wait_result == APP_SEQUENCE_WAIT_DONE)
  {
    (void)appEventClear(APP_EVT_STOP_REQ | APP_EVT_START_REQ | APP_EVT_FOOT_PRESS);
    setState(APP_SEQUENCE_STATE_IDLE_HOME);
    return true;
  }

  if(wait_result == APP_SEQUENCE_WAIT_RESET)
  {
    (void)runResetSequence();
    return false;
  }

  enterErrorState();

  return false;
}

// START 버튼을 눌렀을 때 필요한 전체 시작 시퀀스를 실행한다.
// pump on -> valve on -> servo grab 각도 -> servo home 각도
// end까지 간 후 -> -200
// servo put 각도 -> 붙는 시간 대기 -> servo open 각도
static bool runStartSequence(void)
{
  app_sequence_wait_t wait_result;
  uint32_t cmd_id;

  setState(APP_SEQUENCE_STATE_START_ACTION);

  if(taskPumpOn() != true)
  {
    enterErrorState();
    return false;
  }

  if(taskValveOpen(VALVE1_CH) != true)
  {
    enterErrorState();
    return false;
  }

  if(taskValveOpen(VALVE2_CH) != true)
  {
    enterErrorState();
    return false;
  }

  if(handleResetStopRequest() == true)
  {
    return false;
  }

  if(servoMoveAndWait(SERVO_START_GRAB_BAG_ANGLE_DEG) != true)
  {
    return false;
  }

  if(servoMoveAndWait(SERVO_HOME_ANGLE_DEG) != true)
  {
    return false;
  }

  setState(APP_SEQUENCE_STATE_MOVING_TO_END);

  if(taskStepMotorMoveToEnd(&cmd_id) != true)
  {
    enterErrorState();
    return false;
  }

  wait_result = waitStepMotor(cmd_id);

  if(wait_result == APP_SEQUENCE_WAIT_RESET)
  {
    return runResetSequence();
  }

  if(wait_result == APP_SEQUENCE_WAIT_STOP)
  {
    (void)appEventClear(APP_EVT_STOP_REQ);
    return runStopSequence();
  }

  if(wait_result != APP_SEQUENCE_WAIT_DONE)
  {
    enterErrorState();
    return false;
  }

  setState(APP_SEQUENCE_STATE_END_ACTION);
  if(servoMoveAndWait(SERVO_START_PUT_BAG_ANGLE_DEG) != true)
  {
    return false;
  }

  if(delayInterruptible(SERVO_PUT_SETTLE_DELAY_MS) != true)
  {
    return false;
  }

  if(servoMoveAndWait(SERVO_START_OPEN_BAG_ANGLE_DEG) != true)
  {
    return false;
  }

  (void)appEventClear(APP_EVT_START_REQ | APP_EVT_FOOT_PRESS);
  setState(APP_SEQUENCE_STATE_READY_SEQUENCE);

  return true;
}

// FOOT 스위치를 눌렀을 때 반복 장비 시퀀스를 처리한다.
static bool runFootSwitchSequence(void)
{
  if(app_sequence_state != APP_SEQUENCE_STATE_READY_SEQUENCE)
  {
    return false;
  }

  setState(APP_SEQUENCE_STATE_RUNNING_SEQUENCE);

  if(runStopSequence() != true)
  {
    return false;
  }

  if(runStartSequence() != true)
  {
    return false;
  }

  (void)appEventClear(APP_EVT_START_REQ | APP_EVT_FOOT_PRESS);

  return true;
}

// START 이후 상태에서만 STOP 시퀀스를 실행할 수 있는지 확인한다.
static bool isStopSequenceAllowed(void)
{
  switch(app_sequence_state)
  {
    case APP_SEQUENCE_STATE_START_ACTION:
    case APP_SEQUENCE_STATE_MOVING_TO_END:
    case APP_SEQUENCE_STATE_END_ACTION:
    case APP_SEQUENCE_STATE_READY_SEQUENCE:
    case APP_SEQUENCE_STATE_RUNNING_SEQUENCE:
      return true;

    default:
      return false;
  }
}

// 에러 발생 시 모든 구동부를 안전 상태로 내리고 ERROR 상태로 고정한다.
static void enterErrorState(void)
{
  stopAllActuators();
  (void)appEventClear(CONTROL_EVT);
  setState(APP_SEQUENCE_STATE_ERROR);
}

// reset/error 시 모든 구동부를 안전한 기본 상태로 되돌린다.
static void stopAllActuators(void)
{
  (void)taskStepMotorStop(NULL);
  (void)taskPumpOff();

#ifdef _USE_BTS7960
  for(uint8_t i = 0; i < BTS7960_MAX_CH; i++)
  {
    (void)taskDcMotorStop(i);
  }
#endif

#ifdef _USE_DS3120MG
  for(uint8_t i = 0; i < DS3120MG_MAX_CH; i++)
  {
    (void)taskServoRun(i, SERVO_HOME_ANGLE_DEG);
  }
#endif

#ifdef _USE_2V025
  for(uint8_t i = 0; i < V025_MAX_CH; i++)
  {
    // 부팅/reset 초기 정리에서는 발열을 막기 위해 V025 코일을 강제로 OFF 상태로 둔다.
    (void)taskValveSet(i, false);
  }
#endif
}

// 긴 동작 중 RESET 또는 STOP 요청이 들어왔는지 확인하고 우선 처리한다.
static bool handleResetStopRequest(void)
{
  uint32_t evt;

  evt = appEventGet();

  if((evt & APP_EVT_RESET_REQ) != 0U)
  {
    (void)runResetSequence();
    return true;
  }

  if((evt & APP_EVT_STOP_REQ) != 0U)
  {
    if(isStopSequenceAllowed() == true)
    {
      (void)appEventClear(APP_EVT_STOP_REQ);
      (void)runStopSequence();
      return true;
    }

    (void)appEventClear(APP_EVT_STOP_REQ);
  }

  return false;
}

// 서보를 지정 각도로 이동시키고, 이동 시간 동안 reset/stop을 감시한다.
static bool servoMoveAndWait(float angle_deg)
{
  if(handleResetStopRequest() == true)
  {
    return false;
  }

  if(taskServoRun(SERVO_CH, angle_deg) != true)
  {
    enterErrorState();
    return false;
  }

  if(delayInterruptible(SERVO_WAIT_MS) != true)
  {
    return false;
  }

  return true;
}

// 긴 대기 시간을 짧게 쪼개 reset/stop 요청에 반응할 수 있게 한다.
static bool delayInterruptible(uint32_t delay_ms)
{
  while(delay_ms > 0U)
  {
    uint32_t wait_ms = (delay_ms > ACK_WAIT_MS) ? ACK_WAIT_MS : delay_ms;

    if(handleResetStopRequest() == true)
    {
      return false;
    }

    osDelay(wait_ms);
    delay_ms -= wait_ms;
  }

  return handleResetStopRequest() != true;
}

// 스텝모터 명령 ACK를 기다리면서 reset/stop 요청을 함께 감시한다.
static app_sequence_wait_t waitStepMotor(uint32_t cmd_id)
{
  while(1)
  {
    rtos_step_motor_ack_t ack;
    uint32_t evt;

    evt = appEventGet();

    if((evt & APP_EVT_RESET_REQ) != 0U)
    {
      if(app_sequence_state == APP_SEQUENCE_STATE_HOMING)
      {
        (void)appEventClear(APP_EVT_RESET_REQ);
        continue;
      }

      return APP_SEQUENCE_WAIT_RESET;
    }

    if((evt & APP_EVT_STOP_REQ) != 0U)
    {
      if(isStopSequenceAllowed() == true)
      {
        return APP_SEQUENCE_WAIT_STOP;
      }

      (void)appEventClear(APP_EVT_STOP_REQ);
    }

    if(taskStepMotorGetAck(&ack, ACK_WAIT_MS) != true)
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

// 현재 시퀀스 상태를 갱신한다.
static void setState(app_sequence_state_t state)
{
  app_sequence_state = state;
}
