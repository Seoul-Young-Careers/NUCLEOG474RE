/*
 * task_stepmotor.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/task_stepmotor.h"
#include "task/app_event.h"
#include "task/task_sensor.h"

#define STEP_MOTOR_IDLE_MS                    1        // step motor task가 할 일이 없을 때 쉬는 시간

#define STEP_MOTOR_MOVE_MAX_STEPS             27600    // 센서 탐색 시 안전상 허용하는 최대 이동 step
#define STEP_MOTOR_SENSOR_SLOW_STEPS          2000     // 시작/끝부분을 slow+mid 구간으로 보는 step 수
#define STEP_MOTOR_FULL_MOVE_STEPS            20000    // Zero <-> Full 이동에 넣을 고정 pulse 수

#define STEP_MOTOR_SENSOR_FAST_FREQ_HZ        20000    // 중간 빠른 구간 STEP 주파수
#define STEP_MOTOR_SENSOR_MID_FREQ_HZ         16000    // slow와 fast 사이 중간 구간 STEP 주파수
#define STEP_MOTOR_SENSOR_SLOW_FREQ_HZ        6400     // 출발/도착 근처 slow 구간 STEP 주파수
#define STEP_MOTOR_CALIBRATION_FREQ_HZ        12800    // Calibration 센서 탐색 STEP 주파수

#define STEP_MOTOR_SENSOR_FAST_CHUNK_STEPS    50       // fast 구간에서 한 번에 출력할 step 수
#define STEP_MOTOR_SENSOR_SLOW_CHUNK_STEPS    10       // slow/mid 구간에서 한 번에 출력할 step 수
#define STEP_MOTOR_CALIBRATION_CHUNK_STEPS    1        // Calibration에서 한 번에 출력할 step 수

#define STEP_MOTOR_FREQ_TO_DELAY_US(freq_hz)  (1000000 / (freq_hz))

#define STEP_MOTOR_SENSOR_FAST_DELAY_US       STEP_MOTOR_FREQ_TO_DELAY_US(STEP_MOTOR_SENSOR_FAST_FREQ_HZ)
#define STEP_MOTOR_SENSOR_MID_DELAY_US        STEP_MOTOR_FREQ_TO_DELAY_US(STEP_MOTOR_SENSOR_MID_FREQ_HZ)
#define STEP_MOTOR_SENSOR_SLOW_DELAY_US       STEP_MOTOR_FREQ_TO_DELAY_US(STEP_MOTOR_SENSOR_SLOW_FREQ_HZ)
#define STEP_MOTOR_CALIBRATION_DELAY_US       STEP_MOTOR_FREQ_TO_DELAY_US(STEP_MOTOR_CALIBRATION_FREQ_HZ)

typedef enum
{
  STEP_MOTOR_CALIBRATION_PHASE_NONE = 0,
  STEP_MOTOR_CALIBRATION_PHASE_TO_END,
  STEP_MOTOR_CALIBRATION_PHASE_TO_HOME,
} step_motor_calibration_phase_t;

#define STEP_MOTOR_HOME_DIR            (-1)
#define STEP_MOTOR_END_DIR             1

#define STEP_MOTOR_HOME_SENSOR_EVT     APP_EVT_SN04_1_DETECTED
#define STEP_MOTOR_END_SENSOR_EVT      APP_EVT_SN04_2_DETECTED
#define STEP_MOTOR_ANY_SENSOR_EVT      (STEP_MOTOR_HOME_SENSOR_EVT | STEP_MOTOR_END_SENSOR_EVT)

static osMessageQueueId_t 	step_motor_msg_q = NULL;
static osMessageQueueId_t 	step_motor_ack_q = NULL;
static uint32_t 			step_motor_cmd_id = 0U;

// 외부에서 호출하는 public API는 task_stepmotor.h에 선언하고,
// 아래 함수들은 task_stepmotor.c 내부 상태를 직접 다루는 private helper로 유지한다.
static void 	threadStepMotor(void *argument);
static uint32_t taskStepMotorNextCmdId(void);
static bool 	taskStepMotorPutMsg(rtos_step_motor_msg_t *p_msg, uint32_t *p_cmd_id, bool clear_queue);
static void 	taskStepMotorSendAck(const rtos_step_motor_msg_t *p_msg, rtos_step_motor_ack_result_t result);
static void 	taskStepMotorStopCurrent(uint8_t ch);
static uint32_t taskStepMotorAbsStep(int32_t step);

static uint32_t taskStepMotorGetSensorMoveDelay(uint32_t done_step, uint32_t profile_step, uint32_t default_delay_us);
static uint32_t taskStepMotorGetSensorMoveChunk(uint32_t done_step, uint32_t profile_step, uint32_t remain_step);

// DM542 비동기 pulse 출력 완료를 step motor task 이벤트로 바꾸는 ISR 콜백
static void 	taskStepMotorDm542DoneIsr(uint8_t ch);

#ifdef _USE_SN04
static bool 	taskStepMotorIsTargetDetected(uint32_t target_evt);
static uint32_t taskStepMotorGetDetectedSensorEvt(void);
static uint32_t taskStepMotorGetBlockedSensorEvt(int32_t step);
static uint32_t taskStepMotorGetIgnoreSensorEvt(int32_t step, uint32_t detected_evt);
static bool 	taskStepMotorPrepareLimitMove(int32_t step, uint32_t *p_ignore_evt);
static void 	taskStepMotorUpdateIgnoreSensorEvt(uint32_t *p_ignore_evt, uint32_t detected_evt);
#endif

bool taskStepMotorInit(void)
{
  step_motor_msg_q = osMessageQueueNew(_HW_DEF_RTOS_MSG_Q_STEP_MOTOR,
                                       sizeof(rtos_step_motor_msg_t),
                                       rtosGetStepMotorMsgQAttr());

  if(step_motor_msg_q == NULL)		return false;

  step_motor_ack_q = osMessageQueueNew(_HW_DEF_RTOS_MSG_Q_STEP_MOTOR_ACK,
                                       sizeof(rtos_step_motor_ack_t),
                                       rtosGetStepMotorAckQAttr());

  if(step_motor_ack_q == NULL)		return false;

  // DM542 async 이동이 끝나면 APP_EVT_STEP_MOTOR_DONE 이벤트를 set하도록 등록한다.
  if(dm542AttachDoneCallback(_DEF_DM542_1, taskStepMotorDm542DoneIsr) != true)		return false;

  if(osThreadNew(threadStepMotor, NULL, rtosGetStepMotorThreadAttr()) == NULL)		return false;

  return true;
}


bool taskStepMotorMoveToZero(uint32_t *p_cmd_id)
{
  rtos_step_motor_msg_t msg;

  msg.cmd            = RTOS_STEP_MOTOR_MOVE_TO_ZERO;
  msg.ch             = _DEF_DM542_1;
  msg.step           = STEP_MOTOR_HOME_DIR * (int32_t)STEP_MOTOR_FULL_MOVE_STEPS;
  msg.pulse_delay_us = STEP_MOTOR_SENSOR_FAST_DELAY_US;

  return taskStepMotorPutMsg(&msg, p_cmd_id, true);
}

bool taskStepMotorMoveToFull(uint32_t *p_cmd_id)
{
  rtos_step_motor_msg_t msg;

  msg.cmd            = RTOS_STEP_MOTOR_MOVE_TO_FULL;
  msg.ch             = _DEF_DM542_1;
  msg.step           = STEP_MOTOR_END_DIR * (int32_t)STEP_MOTOR_FULL_MOVE_STEPS;
  msg.pulse_delay_us = STEP_MOTOR_SENSOR_FAST_DELAY_US;

  return taskStepMotorPutMsg(&msg, p_cmd_id, true);
}

bool taskStepMotorMoveToHome(uint32_t *p_cmd_id)
{
  rtos_step_motor_msg_t msg;

  msg.cmd            = RTOS_STEP_MOTOR_MOVE_TO_HOME;
  msg.ch             = _DEF_DM542_1;
  msg.step           = STEP_MOTOR_HOME_DIR * STEP_MOTOR_MOVE_MAX_STEPS;
  msg.pulse_delay_us = STEP_MOTOR_SENSOR_FAST_DELAY_US;

  return taskStepMotorPutMsg(&msg, p_cmd_id, true);
}

bool taskStepMotorMoveToEnd(uint32_t *p_cmd_id)
{
  rtos_step_motor_msg_t msg;

  msg.cmd            = RTOS_STEP_MOTOR_MOVE_TO_END;
  msg.ch             = _DEF_DM542_1;
  msg.step           = STEP_MOTOR_END_DIR * STEP_MOTOR_MOVE_MAX_STEPS;
  msg.pulse_delay_us = STEP_MOTOR_SENSOR_FAST_DELAY_US;

  return taskStepMotorPutMsg(&msg, p_cmd_id, true);
}

bool taskStepMotorCalibration(uint32_t *p_cmd_id)
{
  rtos_step_motor_msg_t msg;

  msg.cmd            = RTOS_STEP_MOTOR_CALIBRATION;
  msg.ch             = _DEF_DM542_1;
  msg.step           = 0;
  msg.pulse_delay_us = STEP_MOTOR_CALIBRATION_DELAY_US;

  return taskStepMotorPutMsg(&msg, p_cmd_id, true);
}

bool taskStepMotorStop(uint32_t *p_cmd_id)
{
  rtos_step_motor_msg_t msg;

  msg.cmd            = RTOS_STEP_MOTOR_STOP;
  msg.ch             = _DEF_DM542_1;
  msg.step           = 0;
  msg.pulse_delay_us = STEP_MOTOR_SENSOR_FAST_DELAY_US;

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

  if(step_motor_msg_q == NULL) 	return false;
  if(step_motor_ack_q == NULL) 	return false;
  if(p_msg == NULL) 			return false;

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
  // msg는 큐에서 방금 꺼낸 새 명령이고, active_msg는 현재 실행 중인 명령이다.
  // ACK는 active_msg.cmd_id를 기준으로 보내서 이전/새 명령이 섞이지 않게 한다.
  rtos_step_motor_msg_t msg;
  rtos_step_motor_msg_t active_msg = {0};

  // active_msg가 실제로 실행 중인지 나타낸다.
  bool has_active_msg = false;

  // 현재 이동에 사용할 DM542 채널이다. 지금 하드웨어는 1채널 기준이다.
  uint8_t move_ch = _DEF_DM542_1;

  // 남은 이동량이다. 부호가 방향을 의미한다. 양수는 End 방향, 음수는 Home 방향이다.
  int32_t move_remain_step = 0;

  // 현재 이동 chunk에 적용할 STEP pulse 주기(us)다.
  uint32_t move_pulse_delay_us = STEP_MOTOR_SENSOR_FAST_DELAY_US;

  // 속도 프로파일 계산용 진행 step 수다. chunk가 끝날 때마다 증가한다.
  uint32_t move_done_step = 0U;

  // 속도 프로파일을 적용할 전체 이동 step 수다.
  uint32_t move_profile_step = 0U;

  // Home/End 센서를 목표로 움직이는 명령일 때, 정상 완료로 인정할 센서 이벤트다.
  uint32_t target_evt = 0U;

  // true면 slow/mid/fast 속도 프로파일을 적용한다.
  // false면 calibration처럼 고정 속도로 움직인다.
  bool use_sensor_profile = false;

  // Calibration은 End를 먼저 찾고 Home을 찾는 2단계라서 현재 단계를 따로 저장한다.
  step_motor_calibration_phase_t calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_NONE;

  // Timer PWM으로 실행 중인 target 이동 chunk가 있는지 표시한다.
  bool is_async_chunk_running = false;

  // async chunk 완료 후 remain_step을 줄이기 위해 시작한 chunk step을 저장한다.
  int32_t async_chunk_step = 0;
#ifdef _USE_SN04
  // 이미 눌린 limit 센서에서 빠져나가는 중이면 그 센서는 release 전까지 정지 조건에서 제외한다.
  uint32_t ignore_sensor_evt = 0U;
#endif

  UNUSED(argument);

  while(1)
  {
    // 새 명령이 들어왔는지 먼저 확인한다. timeout 0이라서 없으면 바로 아래 처리로 넘어간다.
    if(osMessageQueueGet(step_motor_msg_q, &msg, NULL, 0U) == osOK)
    {
      if(has_active_msg == true)
      {
        if(is_async_chunk_running == true)
        {
          // 새 명령이 들어오면 진행 중인 async PWM chunk를 먼저 중단한다.
          taskStepMotorStopCurrent(move_ch);
          is_async_chunk_running = false;
          async_chunk_step = 0;
        }

        taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_STOPPED);
      }

      // 새 명령을 현재 실행 명령으로 등록하고, 이전 명령의 이동 상태를 모두 초기화한다.
      active_msg 				= msg;
      has_active_msg 			= true;
      move_ch 					= msg.ch;
      move_pulse_delay_us 		= msg.pulse_delay_us;
      move_done_step 			= 0U;
      move_profile_step 		= 0U;
      target_evt 				= 0U;
      use_sensor_profile 		= false;
      calibration_phase 		= STEP_MOTOR_CALIBRATION_PHASE_NONE;
      is_async_chunk_running 	= false;
      async_chunk_step = 0;
#ifdef _USE_SN04
      ignore_sensor_evt = 0U;
      taskSensorSetDm542StopIgnore(ignore_sensor_evt);
#endif
      // 이전 async 완료 이벤트가 새 명령에 섞이지 않도록 비운다.
      (void)appEventClear(APP_EVT_STEP_MOTOR_DONE);

      switch(msg.cmd)
      {
        case RTOS_STEP_MOTOR_MOVE_TO_ZERO:
        case RTOS_STEP_MOTOR_MOVE_TO_FULL:
          // Zero/Full은 소프트웨어 좌표 보정 없이 지정된 pulse 수만 그대로 출력한다.
          // Zero는 -STEP_MOTOR_FULL_MOVE_STEPS, Full은 +STEP_MOTOR_FULL_MOVE_STEPS다.
          target_evt 			= 0U;
          use_sensor_profile 	= true;
          move_remain_step 		= msg.step;
          move_profile_step 	= taskStepMotorAbsStep(move_remain_step);

          if(move_remain_step == 0)
          {
            taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
            has_active_msg = false;
          }
#ifdef _USE_SN04
          else if(taskStepMotorPrepareLimitMove(move_remain_step, &ignore_sensor_evt) != true)
          {
            move_remain_step = 0;
            taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
            has_active_msg = false;
          }
#endif
          break;

        case RTOS_STEP_MOTOR_MOVE_TO_HOME:
#ifdef _USE_SN04
          // Home 센서를 목표로 이동한다. Home 센서가 잡히면 정상 완료다.
          target_evt = STEP_MOTOR_HOME_SENSOR_EVT;
          use_sensor_profile = true;
          move_profile_step = taskStepMotorAbsStep(msg.step);

          if(taskStepMotorIsTargetDetected(target_evt) == true)
          {
            move_remain_step = 0;
            taskStepMotorStopCurrent(move_ch);
            taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
            has_active_msg = false;
          }
          else
          {
            move_remain_step = msg.step;
            if(taskStepMotorPrepareLimitMove(move_remain_step, &ignore_sensor_evt) != true)
            {
              move_remain_step = 0;
              taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
              has_active_msg = false;
            }
          }
#else
          move_remain_step = 0;
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
          has_active_msg = false;
#endif
          break;

        case RTOS_STEP_MOTOR_MOVE_TO_END:
#ifdef _USE_SN04
          // End 센서를 목표로 이동한다. End 센서가 잡히면 정상 완료다.
          target_evt = STEP_MOTOR_END_SENSOR_EVT;
          use_sensor_profile = true;
          move_profile_step = taskStepMotorAbsStep(msg.step);

          if(taskStepMotorIsTargetDetected(target_evt) == true)
          {
            move_remain_step = 0;
            taskStepMotorStopCurrent(move_ch);
            taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
            has_active_msg = false;
          }
          else
          {
            move_remain_step = msg.step;
            if(taskStepMotorPrepareLimitMove(move_remain_step, &ignore_sensor_evt) != true)
            {
              move_remain_step = 0;
              taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
              has_active_msg = false;
            }
          }
#else
          move_remain_step = 0;
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
          has_active_msg = false;
#endif
          break;

        case RTOS_STEP_MOTOR_CALIBRATION:
#ifdef _USE_SN04
          // Calibration은 위치 기준을 다시 잡기 위한 명령이다.
          // 먼저 End 센서를 찾고, 그 다음 Home 센서를 찾아 Home 위치를 0 step으로 잡는다.
          // 속도 프로파일은 쓰지 않고 12800Hz 고정 속도로 움직인다.
          target_evt 			= STEP_MOTOR_END_SENSOR_EVT;
          use_sensor_profile 	= false;
          calibration_phase 	= STEP_MOTOR_CALIBRATION_PHASE_TO_END;
          move_pulse_delay_us 	= STEP_MOTOR_CALIBRATION_DELAY_US;
          move_profile_step 	= taskStepMotorAbsStep(STEP_MOTOR_END_DIR * STEP_MOTOR_MOVE_MAX_STEPS);

          if(taskStepMotorIsTargetDetected(target_evt) == true)
          {
            // 이미 End 센서가 감지된 상태면 바로 Home 센서를 찾는 phase로 넘어간다.
            target_evt 			= STEP_MOTOR_HOME_SENSOR_EVT;
            calibration_phase 	= STEP_MOTOR_CALIBRATION_PHASE_TO_HOME;
            move_profile_step 	= taskStepMotorAbsStep(STEP_MOTOR_HOME_DIR * STEP_MOTOR_MOVE_MAX_STEPS);
            move_remain_step 	= STEP_MOTOR_HOME_DIR * STEP_MOTOR_MOVE_MAX_STEPS;
          }
          else
          {
            move_remain_step = STEP_MOTOR_END_DIR * STEP_MOTOR_MOVE_MAX_STEPS;
          }

          if((has_active_msg == true) &&
             (taskStepMotorPrepareLimitMove(move_remain_step, &ignore_sensor_evt) != true))
          {
            move_remain_step = 0;
            taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
            has_active_msg = false;
          }
#else
          move_remain_step = 0;
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
          has_active_msg = false;
#endif
          break;

        case RTOS_STEP_MOTOR_STOP:
          // 현재 이동을 중단하고 DONE ACK를 보낸다. 실제 정지는 taskStepMotorStopCurrent()에서 처리한다.
          move_remain_step = 0;
          taskStepMotorStopCurrent(move_ch);
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
          has_active_msg = false;
          break;

        default:
          // 정의되지 않은 명령은 실행하지 않고 ERROR ACK로 종료한다.
          move_remain_step = 0;
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
          has_active_msg = false;
          break;
      }
    }

#ifdef _USE_SN04
    // 현재 센서 상태를 읽고, 빠져나가는 중이라 무시하기로 한 센서는 release 전까지 제외한다.
    // 예: Home 센서 위에서 End 방향으로 빠져나갈 때 Home 센서는 ignore 대상이다.
    uint32_t detected_sensor_evt 	= taskStepMotorGetDetectedSensorEvt();
    taskStepMotorUpdateIgnoreSensorEvt(&ignore_sensor_evt, detected_sensor_evt);
    detected_sensor_evt 			&= ~ignore_sensor_evt;

    if((has_active_msg == true) && (detected_sensor_evt != 0U))
    {
      if(is_async_chunk_running == true)
      {
        // 센서 감지 시 PWM은 ISR에서 이미 멈췄을 수 있고, 여기서는 task 상태와 ACK를 정리한다.
        taskStepMotorStopCurrent(move_ch);
        is_async_chunk_running = false;
        async_chunk_step = 0;
      }

      if((target_evt == 0U) || ((detected_sensor_evt & target_evt) == 0U))
      {
        // 목표 센서 이동이 아니거나 목표가 아닌 센서가 감지되면 limit 충돌로 본다.
        // DM542는 ISR에서 이미 멈췄고, 여기서는 명령을 ERROR로 정리한다.
        move_remain_step = 0;
        taskStepMotorStopCurrent(move_ch);
        taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
        calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_NONE;
        has_active_msg = false;
        osDelay(STEP_MOTOR_IDLE_MS);
        continue;
      }

      if((active_msg.cmd == RTOS_STEP_MOTOR_CALIBRATION) &&
         (calibration_phase == STEP_MOTOR_CALIBRATION_PHASE_TO_END))
      {
        // Calibration 1단계: End 센서를 찾았다.
        // 같은 명령 안에서 목표 센서를 Home으로 바꾸고 반대 방향으로 다시 움직인다.
        target_evt = STEP_MOTOR_HOME_SENSOR_EVT;
        calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_TO_HOME;
        move_pulse_delay_us = STEP_MOTOR_CALIBRATION_DELAY_US;
        move_done_step = 0U;
        move_profile_step = taskStepMotorAbsStep(STEP_MOTOR_HOME_DIR * STEP_MOTOR_MOVE_MAX_STEPS);
        move_remain_step = STEP_MOTOR_HOME_DIR * STEP_MOTOR_MOVE_MAX_STEPS;
        if(taskStepMotorPrepareLimitMove(move_remain_step, &ignore_sensor_evt) != true)
        {
          move_remain_step = 0;
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
          calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_NONE;
          has_active_msg = false;
        }

        (void)appEventClear(APP_EVT_STEP_MOTOR_DONE);
        osDelay(STEP_MOTOR_IDLE_MS);
        continue;
      }

      if((active_msg.cmd == RTOS_STEP_MOTOR_CALIBRATION) &&
         (calibration_phase == STEP_MOTOR_CALIBRATION_PHASE_TO_HOME))
      {
        // Calibration 2단계: Home 센서를 찾았다.
        // 이 위치를 기계 원점으로 보고 소프트웨어 position_step을 0으로 맞춘다.
        move_remain_step = 0;
        (void)dm542SetPositionStep(move_ch, 0);
        taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
        calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_NONE;
        has_active_msg = false;
        osDelay(STEP_MOTOR_IDLE_MS);
        continue;
      }

      move_remain_step = 0;
      taskStepMotorStopCurrent(move_ch);
      taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
      calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_NONE;
      taskSensorSetDm542StopIgnore(0U);
      has_active_msg = false;
      osDelay(STEP_MOTOR_IDLE_MS);
      continue;
    }
#endif

    if(move_remain_step != 0)
    {
      uint32_t chunk_step = 1U;
      int32_t step;
      uint32_t pulse_delay_us = move_pulse_delay_us;

      if(is_async_chunk_running == true)
      {
        uint32_t evt_flags;

        // 센서 정지는 ISR에서 즉시 처리하고, task는 PWM 완료만 짧게 기다린다.
        evt_flags = appEventWait(APP_EVT_STEP_MOTOR_DONE,
                                 osFlagsWaitAny | osFlagsNoClear,
                                 STEP_MOTOR_IDLE_MS);

        if((evt_flags & osFlagsError) != 0U)
        {
          osDelay(STEP_MOTOR_IDLE_MS);
        }
        else if((evt_flags & APP_EVT_STEP_MOTOR_DONE) != 0U)
        {
          (void)appEventClear(APP_EVT_STEP_MOTOR_DONE);

          // async chunk가 정상 완료됐다.
          // 시작할 때 저장해 둔 async_chunk_step만큼 남은 이동량과 프로파일 진행량을 갱신한다.
          move_remain_step -= async_chunk_step;
          move_done_step += taskStepMotorAbsStep(async_chunk_step);
          is_async_chunk_running = false;
          async_chunk_step = 0;

          if(move_remain_step == 0)
          {
            if(target_evt == 0U)
            {
              // Zero/Full 고정 pulse 이동은 step 완료가 정상 종료 조건이다.
              taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
              calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_NONE;
            }
            else
            {
              // Home/End 센서를 찾지 못한 채 최대 이동 step을 소진하면 비정상 종료로 처리한다.
              taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
            }

            has_active_msg = false;
          }
        }
        else
        {
          osDelay(STEP_MOTOR_IDLE_MS);
        }
      }
      else
      {
        uint32_t remain_abs_step = taskStepMotorAbsStep(move_remain_step);

        if(use_sensor_profile == true)
        {
          // 좌표 이동/센서 이동은 진행 위치에 따라 slow/mid/fast 속도와 chunk 크기를 정한다.
          pulse_delay_us = taskStepMotorGetSensorMoveDelay(move_done_step, move_profile_step, move_pulse_delay_us);
          chunk_step = taskStepMotorGetSensorMoveChunk(move_done_step,
                                                       move_profile_step,
                                                       remain_abs_step);
        }
        else
        {
          // Calibration은 센서 기준을 찾는 동작이라 프로파일 없이 1 step chunk로 촘촘하게 움직인다.
          pulse_delay_us = move_pulse_delay_us;
          chunk_step = STEP_MOTOR_CALIBRATION_CHUNK_STEPS;

          if(chunk_step > remain_abs_step)
          {
            chunk_step = remain_abs_step;
          }
        }

        // 이번에 Timer PWM으로 출력할 chunk의 방향과 크기를 정한다.
        step = (move_remain_step > 0) ? (int32_t)chunk_step : -(int32_t)chunk_step;

        // 새 chunk 시작 전에 이전 완료 이벤트가 남아 있지 않게 한다.
        (void)appEventClear(APP_EVT_STEP_MOTOR_DONE);

        if(dm542MoveStepAsync(move_ch, step, pulse_delay_us) == true)
        {
          async_chunk_step = step;
          is_async_chunk_running = true;
        }
        else
        {
          move_remain_step = 0;
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
          has_active_msg = false;
        }
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

#ifdef _USE_SN04
  taskSensorSetDm542StopIgnore(0U);
#endif

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

static uint32_t taskStepMotorAbsStep(int32_t step)
{
  if(step >= 0)
  {
    return (uint32_t)step;
  }

  return (uint32_t)(-(step + 1)) + 1U;
}

static uint32_t taskStepMotorGetSensorMoveDelay(uint32_t done_step, uint32_t profile_step, uint32_t default_delay_us)
{
  uint32_t slow_start_step;

  uint32_t slow_half_step = STEP_MOTOR_SENSOR_SLOW_STEPS / 2U;

  // 단계 방식: 가속/중간/빠른/중간/감속 구간을 고정 주파수 단계로 나눠서 사용한다.
  if(profile_step <= (STEP_MOTOR_SENSOR_SLOW_STEPS * 2U))
  {
    uint32_t accel_step = profile_step / 2U;
    uint32_t decel_step = profile_step - accel_step;
    uint32_t accel_half_step = accel_step / 2U;
    uint32_t decel_half_step = decel_step / 2U;

    if(done_step < accel_half_step)						return STEP_MOTOR_SENSOR_SLOW_DELAY_US;
    if(done_step < accel_step)							return STEP_MOTOR_SENSOR_MID_DELAY_US;
    if((done_step - accel_step) < decel_half_step)      return STEP_MOTOR_SENSOR_MID_DELAY_US;

    return STEP_MOTOR_SENSOR_SLOW_DELAY_US;
  }

  slow_start_step = profile_step - STEP_MOTOR_SENSOR_SLOW_STEPS;

  if(done_step < STEP_MOTOR_SENSOR_SLOW_STEPS)
  {
    if((slow_half_step == 0U) || (done_step < slow_half_step))
    {
      return STEP_MOTOR_SENSOR_SLOW_DELAY_US;
    }

    return STEP_MOTOR_SENSOR_MID_DELAY_US;
  }

  if(done_step >= slow_start_step)
  {
    uint32_t decel_done_step = done_step - slow_start_step;

    if((slow_half_step == 0U) || (decel_done_step < slow_half_step))
    {
      return STEP_MOTOR_SENSOR_MID_DELAY_US;
    }

    return STEP_MOTOR_SENSOR_SLOW_DELAY_US;
  }

  return default_delay_us;
}

static uint32_t taskStepMotorGetSensorMoveChunk(uint32_t done_step, uint32_t profile_step, uint32_t remain_step)
{
  uint32_t chunk_step = STEP_MOTOR_SENSOR_SLOW_CHUNK_STEPS;
  uint32_t slow_start_step;
  uint32_t remain_fast_step;

  if(remain_step == 0U)
  {
    return 0U;
  }

  if(profile_step > (STEP_MOTOR_SENSOR_SLOW_STEPS * 2U))
  {
    slow_start_step = profile_step - STEP_MOTOR_SENSOR_SLOW_STEPS;

    if(done_step < STEP_MOTOR_SENSOR_SLOW_STEPS)
    {
      // 가속 구간은 주파수를 자주 갱신하기 위해 빠른 구간보다 작은 chunk로 움직인다.
      chunk_step = STEP_MOTOR_SENSOR_SLOW_CHUNK_STEPS;
    }
    else if(done_step < slow_start_step)
    {
      remain_fast_step = slow_start_step - done_step;
      // 빠른 구간은 한 번에 더 많은 step을 출력해서 RTOS wake-up 부담을 줄인다.
      chunk_step = STEP_MOTOR_SENSOR_FAST_CHUNK_STEPS;
      if(chunk_step > remain_fast_step)
      {
        // 빠른 구간 끝을 넘어서 감속 구간으로 밀고 들어가지 않도록 chunk를 자른다.
        chunk_step = remain_fast_step;
      }
    }
  }

  if(chunk_step > remain_step)
  {
    // 남은 step보다 큰 chunk를 요청하지 않도록 마지막 chunk 크기를 제한한다.
    chunk_step = remain_step;
  }

  if(chunk_step == 0U)
  {
    chunk_step = 1U;
  }

  return chunk_step;
}

static void taskStepMotorDm542DoneIsr(uint8_t ch)
{
  UNUSED(ch);

  // PWM count 완료 ISR에서 step motor task를 깨우기 위한 이벤트를 set한다.
  (void)appEventSet(APP_EVT_STEP_MOTOR_DONE);
}

#ifdef _USE_SN04
// 목표 센서가 이미 감지된 상태인지 확인한다.
// appEvent는 sensor task가 마지막으로 반영한 상태이고, sn04IsDetected()는 현재 GPIO 상태다.
// 둘 중 하나라도 감지 상태면 목표 위치에 이미 도착한 것으로 보고 이동을 시작하지 않는다.
static bool taskStepMotorIsTargetDetected(uint32_t target_evt)
{
  uint32_t evt_flags = appEventGet();

  if((evt_flags & target_evt) != 0U)
  {
    return true;
  }

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

// 현재 감지 중인 SN04 센서를 event bit 형태로 모아서 반환한다.
// sensor task 이벤트만 보면 갱신 타이밍 차이가 있을 수 있어서 실제 GPIO 상태도 같이 확인한다.
// 반환값은 STEP_MOTOR_HOME_SENSOR_EVT, STEP_MOTOR_END_SENSOR_EVT를 OR한 값이다.
static uint32_t taskStepMotorGetDetectedSensorEvt(void)
{
  uint32_t evt_flags = appEventGet() & STEP_MOTOR_ANY_SENSOR_EVT;

  if(sn04IsDetected(_DEF_SN04_1) == true)
  {
    evt_flags |= STEP_MOTOR_HOME_SENSOR_EVT;
  }

  if(sn04IsDetected(_DEF_SN04_2) == true)
  {
    evt_flags |= STEP_MOTOR_END_SENSOR_EVT;
  }

  return evt_flags;
}

// 이동하려는 방향에서 더 들어가면 안 되는 limit 센서를 반환한다.
// step > 0: End 방향 이동이므로 SN04_2(End)가 이미 감지되어 있으면 차단 대상이다.
// step < 0: Home/Start 방향 이동이므로 SN04_1(Home)이 이미 감지되어 있으면 차단 대상이다.
static uint32_t taskStepMotorGetBlockedSensorEvt(int32_t step)
{
  if(step > 0)
  {
    return STEP_MOTOR_END_SENSOR_EVT;
  }

  if(step < 0)
  {
    return STEP_MOTOR_HOME_SENSOR_EVT;
  }

  return 0U;
}

// 현재 눌려 있지만 이동 방향과 반대편에 있는 센서를 ignore 대상으로 반환한다.
// limit 센서 위에서 빠져나오는 동작은 허용해야 하므로, release될 때까지 해당 센서는 정지 조건에서 제외한다.
// 예: SN04_1(Home)이 눌린 상태에서 End 방향으로 움직이면 SN04_1은 ignore, SN04_2는 그대로 limit이다.
static uint32_t taskStepMotorGetIgnoreSensorEvt(int32_t step, uint32_t detected_evt)
{
  if(step > 0)
  {
    return detected_evt & STEP_MOTOR_HOME_SENSOR_EVT;
  }

  if(step < 0)
  {
    return detected_evt & STEP_MOTOR_END_SENSOR_EVT;
  }

  return 0U;
}

// 새 이동 명령을 시작하기 전에 limit 센서 상태를 검사한다.
// 이동 방향 쪽 limit 센서가 이미 눌려 있으면 더 밀고 들어갈 수 없으므로 false를 반환한다.
// 반대편 센서가 눌려 있으면 빠져나갈 수 있도록 DM542 sensor stop ignore mask에 등록한다.
static bool taskStepMotorPrepareLimitMove(int32_t step, uint32_t *p_ignore_evt)
{
  uint32_t detected_evt;
  uint32_t blocked_evt;
  uint32_t ignore_evt;

  if(p_ignore_evt == NULL) return false;

  detected_evt = taskStepMotorGetDetectedSensorEvt();
  blocked_evt = taskStepMotorGetBlockedSensorEvt(step);
  if((detected_evt & blocked_evt) != 0U)
  {
    taskSensorSetDm542StopIgnore(0U);
    *p_ignore_evt = 0U;
    return false;
  }

  ignore_evt = taskStepMotorGetIgnoreSensorEvt(step, detected_evt);
  taskSensorSetDm542StopIgnore(ignore_evt);
  *p_ignore_evt = ignore_evt;

  return true;
}

// ignore 중인 센서가 아직 눌려 있는지 갱신한다.
// 센서가 release되면 ignore mask에서 제거해서, 다시 감지될 때는 정상 limit 정지 조건으로 동작하게 한다.
static void taskStepMotorUpdateIgnoreSensorEvt(uint32_t *p_ignore_evt, uint32_t detected_evt)
{
  if(p_ignore_evt == NULL) return;

  *p_ignore_evt &= detected_evt;
  taskSensorSetDm542StopIgnore(*p_ignore_evt);
}
#endif
