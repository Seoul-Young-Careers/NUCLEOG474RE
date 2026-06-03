/*
 * task_stepmotor.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/task_stepmotor.h"
#include "task/app_event.h"

#define STEP_MOTOR_IDLE_MS             								1U      // step motor task가 할 일이 없을 때 잠깐 쉬는 시간(ms)
#define STEP_MOTOR_PULSE_DELAY_US      								25600U    // 일반 MoveStep에서 사용하는 기본 STEP pulse 주기(us), 500us는 약 2kHz

#define STEP_MOTOR_TRAVEL_MAX_STEPS    								33000   // Home/End 센서를 찾을 때 안전상 최대로 이동할 수 있는 step 수
#define STEP_MOTOR_SENSOR_TRAVEL_STEPS 								64000U   // 센서 이동 프로파일 계산에 사용하는 기준 이동 거리(step)

#define STEP_MOTOR_READY_OFFSET_STEPS  								800U    // End 이동 시 센서 기준 위치에서 준비 위치만큼 빼기 위한 offset step
#define STEP_MOTOR_END_TRAVEL_STEPS    								(STEP_MOTOR_SENSOR_TRAVEL_STEPS - STEP_MOTOR_READY_OFFSET_STEPS) // End 방향 프로파일 거리에서 준비 offset을 뺀 값
#define STEP_MOTOR_SENSOR_SLOW_STEPS   								1800U    // 이동 시작부/끝부분에서 가속 또는 감속 구간으로 취급할 step 수
#define STEP_MOTOR_FREQ_TO_DELAY_US(freq_hz) 					(1000000U / (freq_hz)) // 주파수(Hz)를 STEP pulse 주기(us)로 변환

#define STEP_MOTOR_SENSOR_FAST_FREQ_HZ  							25600U   // 중간 빠른 구간의 STEP 주파수(Hz), 값이 클수록 빠름
#define STEP_MOTOR_SENSOR_ACCEL_FREQ_HZ 							6400U   // 출발 직후 가속 구간의 STEP 주파수(Hz)
#define STEP_MOTOR_SENSOR_DECEL_FREQ_HZ 							6400U   // 센서 도착 직전 감속 구간의 STEP 주파수(Hz)
#define STEP_MOTOR_SENSOR_MID_FREQ_HZ 								12800U   // 가속/감속 중간에 한 단계 더 거쳐가는 STEP 주파수(Hz)

#define STEP_MOTOR_SENSOR_FAST_DELAY_US 							STEP_MOTOR_FREQ_TO_DELAY_US(STEP_MOTOR_SENSOR_FAST_FREQ_HZ)   // 빠른 구간 주파수를 timer PWM 주기(us)로 변환한 값
#define STEP_MOTOR_SENSOR_ACCEL_DELAY_US 							STEP_MOTOR_FREQ_TO_DELAY_US(STEP_MOTOR_SENSOR_ACCEL_FREQ_HZ)  // 가속 구간 주파수를 timer PWM 주기(us)로 변환한 값
#define STEP_MOTOR_SENSOR_DECEL_DELAY_US 							STEP_MOTOR_FREQ_TO_DELAY_US(STEP_MOTOR_SENSOR_DECEL_FREQ_HZ)  // 감속 구간 주파수를 timer PWM 주기(us)로 변환한 값
#define STEP_MOTOR_SENSOR_MID_DELAY_US 								STEP_MOTOR_FREQ_TO_DELAY_US(STEP_MOTOR_SENSOR_MID_FREQ_HZ)    // 중간 단계 주파수를 timer PWM 주기(us)로 변환한 값

#define STEP_MOTOR_SENSOR_FAST_CHUNK_STEPS 						50U     // 빠른 구간에서 한 번에 timer PWM으로 출력할 step 묶음 크기
#define STEP_MOTOR_SENSOR_ACCEL_CHUNK_STEPS 					10U     // 가속 구간에서 한 번에 timer PWM으로 출력할 step 묶음 크기
#define STEP_MOTOR_SENSOR_DECEL_CHUNK_STEPS 					10U      // 감속 구간에서 한 번에 timer PWM으로 출력할 step 묶음 크기, 작을수록 센서 정지 반응이 촘촘함

#define STEP_MOTOR_CALIBRATION_FREQ_HZ 								12800U    // Calibration 중 End/Home 센서를 찾을 때 사용할 고정 STEP 주파수(Hz)
#define STEP_MOTOR_CALIBRATION_DELAY_US 							STEP_MOTOR_FREQ_TO_DELAY_US(STEP_MOTOR_CALIBRATION_FREQ_HZ) // Calibration 주파수를 timer PWM 주기(us)로 변환한 값
#define STEP_MOTOR_CALIBRATION_CHUNK_STEPS 						1U      // Calibration 중 한 번에 timer PWM으로 출력할 step 묶음 크기
#define STEP_MOTOR_CALIBRATION_READY_OFFSET_STEPS 		1000U     // Calibration 마지막에 Home 센서에서 End 방향으로 더 이동할 준비 위치 offset step

typedef enum
{
  STEP_MOTOR_CALIBRATION_PHASE_NONE = 0,
  STEP_MOTOR_CALIBRATION_PHASE_TO_END,
  STEP_MOTOR_CALIBRATION_PHASE_TO_HOME,
  STEP_MOTOR_CALIBRATION_PHASE_READY_OFFSET,
} step_motor_calibration_phase_t;

#define STEP_MOTOR_HOME_DIR            (-1)
#define STEP_MOTOR_END_DIR             1

#define STEP_MOTOR_HOME_SENSOR_EVT     APP_EVT_SN04_1_DETECTED
#define STEP_MOTOR_END_SENSOR_EVT      APP_EVT_SN04_2_DETECTED

static osMessageQueueId_t step_motor_msg_q = NULL;
static osMessageQueueId_t step_motor_ack_q = NULL;
static uint32_t step_motor_cmd_id = 0U;

static void threadStepMotor(void *argument);
static uint32_t taskStepMotorNextCmdId(void);
static bool taskStepMotorPutMsg(rtos_step_motor_msg_t *p_msg, uint32_t *p_cmd_id, bool clear_queue);
static void taskStepMotorSendAck(const rtos_step_motor_msg_t *p_msg, rtos_step_motor_ack_result_t result);
static void taskStepMotorStopCurrent(uint8_t ch);
static uint32_t taskStepMotorAbsStep(int32_t step);

static uint32_t taskStepMotorGetSensorMoveDelay(uint32_t done_step, uint32_t profile_step, uint32_t default_delay_us);
static uint32_t taskStepMotorGetSensorMoveChunk(uint32_t done_step, uint32_t profile_step, uint32_t remain_step);

// DM542 비동기 pulse 출력 완료를 step motor task 이벤트로 바꾸는 ISR 콜백
static void taskStepMotorDm542DoneIsr(uint8_t ch);

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

  // DM542 async 이동이 끝나면 APP_EVT_STEP_MOTOR_DONE 이벤트를 set하도록 등록한다.
  if(dm542AttachDoneCallback(_DEF_DM542_1, taskStepMotorDm542DoneIsr) != true)
  {
    logPrintf("stepMotorDoneCb \t: Fail\r\n");
    return false;
  }

  logPrintf("stepMotorDoneCb \t: OK\r\n");

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
  msg.pulse_delay_us = STEP_MOTOR_SENSOR_FAST_DELAY_US;

  return taskStepMotorPutMsg(&msg, p_cmd_id, true);
}

bool taskStepMotorMoveToEnd(uint32_t *p_cmd_id)
{
  rtos_step_motor_msg_t msg;

  msg.cmd            = RTOS_STEP_MOTOR_CMD_MOVE_TO_END;
  msg.ch             = _DEF_DM542_1;
  msg.step           = STEP_MOTOR_END_DIR * STEP_MOTOR_TRAVEL_MAX_STEPS;
  msg.pulse_delay_us = STEP_MOTOR_SENSOR_FAST_DELAY_US;

  return taskStepMotorPutMsg(&msg, p_cmd_id, true);
}

bool taskStepMotorCalibration(uint32_t *p_cmd_id)
{
  rtos_step_motor_msg_t msg;

  msg.cmd            = RTOS_STEP_MOTOR_CMD_CALIBRATION;
  msg.ch             = _DEF_DM542_1;
  msg.step           = 0;
  msg.pulse_delay_us = STEP_MOTOR_CALIBRATION_DELAY_US;

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
  uint32_t move_done_step = 0U;
  uint32_t move_profile_step = STEP_MOTOR_SENSOR_TRAVEL_STEPS;
  uint32_t target_evt = 0U;
  bool is_target_move = false;
  bool use_sensor_profile = false;
  step_motor_calibration_phase_t calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_NONE;
  // Timer PWM으로 실행 중인 target 이동 chunk가 있는지 표시한다.
  bool is_async_chunk_running = false;
  // async chunk 완료 후 remain_step을 줄이기 위해 시작한 chunk step을 저장한다.
  int32_t async_chunk_step = 0;

  UNUSED(argument);

  while(1)
  {
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

      active_msg = msg;
      has_active_msg = true;
      move_ch = msg.ch;
      move_pulse_delay_us = msg.pulse_delay_us;
      move_done_step = 0U;
      move_profile_step = STEP_MOTOR_SENSOR_TRAVEL_STEPS;
      target_evt = 0U;
      is_target_move = false;
      use_sensor_profile = false;
      calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_NONE;
      is_async_chunk_running = false;
      async_chunk_step = 0;
      // 이전 async 완료 이벤트가 새 명령에 섞이지 않도록 비운다.
      (void)appEventClear(APP_EVT_STEP_MOTOR_DONE);

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
          target_evt = STEP_MOTOR_HOME_SENSOR_EVT;
          is_target_move = true;
          use_sensor_profile = true;
          move_profile_step = STEP_MOTOR_SENSOR_TRAVEL_STEPS;
          if(move_profile_step > taskStepMotorAbsStep(msg.step))
          {
            move_profile_step = taskStepMotorAbsStep(msg.step);
          }

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
          }
#else
          move_remain_step = 0;
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
          has_active_msg = false;
#endif
          break;

        case RTOS_STEP_MOTOR_CMD_MOVE_TO_END:
#ifdef _USE_SN04
          target_evt = STEP_MOTOR_END_SENSOR_EVT;
          is_target_move = true;
          use_sensor_profile = true;
          move_profile_step = STEP_MOTOR_END_TRAVEL_STEPS;
          if(move_profile_step > taskStepMotorAbsStep(msg.step))
          {
            move_profile_step = taskStepMotorAbsStep(msg.step);
          }

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
          }
#else
          move_remain_step = 0;
          taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_ERROR);
          has_active_msg = false;
#endif
          break;

        case RTOS_STEP_MOTOR_CMD_CALIBRATION:
#ifdef _USE_SN04
          // Calibration은 기존 MoveToEnd/MoveToHome 프로파일을 쓰지 않고 고정 속도로 End -> Home -> 준비 위치 순서로 이동한다.
          target_evt = STEP_MOTOR_END_SENSOR_EVT;
          is_target_move = true;
          use_sensor_profile = false;
          calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_TO_END;
          move_pulse_delay_us = STEP_MOTOR_CALIBRATION_DELAY_US;
          move_profile_step = taskStepMotorAbsStep(STEP_MOTOR_END_DIR * STEP_MOTOR_TRAVEL_MAX_STEPS);

          if(taskStepMotorIsTargetDetected(target_evt) == true)
          {
            // 이미 End 센서가 감지된 상태면 바로 Home 센서를 찾는 phase로 넘어간다.
            target_evt = STEP_MOTOR_HOME_SENSOR_EVT;
            calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_TO_HOME;
            move_profile_step = taskStepMotorAbsStep(STEP_MOTOR_HOME_DIR * STEP_MOTOR_TRAVEL_MAX_STEPS);
            move_remain_step = STEP_MOTOR_HOME_DIR * STEP_MOTOR_TRAVEL_MAX_STEPS;
          }
          else
          {
            move_remain_step = STEP_MOTOR_END_DIR * STEP_MOTOR_TRAVEL_MAX_STEPS;
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
      if(is_async_chunk_running == true)
      {
        // 센서 감지 시 PWM은 ISR에서 이미 멈췄을 수 있고, 여기서는 task 상태와 ACK를 정리한다.
        taskStepMotorStopCurrent(move_ch);
        is_async_chunk_running = false;
        async_chunk_step = 0;
      }

      if((active_msg.cmd == RTOS_STEP_MOTOR_CMD_CALIBRATION) &&
         (calibration_phase == STEP_MOTOR_CALIBRATION_PHASE_TO_END))
      {
        // Calibration은 End 센서를 찾은 뒤 같은 명령 안에서 Home 센서를 한 번 더 찾는다.
        target_evt = STEP_MOTOR_HOME_SENSOR_EVT;
        calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_TO_HOME;
        move_pulse_delay_us = STEP_MOTOR_CALIBRATION_DELAY_US;
        move_done_step = 0U;
        move_profile_step = taskStepMotorAbsStep(STEP_MOTOR_HOME_DIR * STEP_MOTOR_TRAVEL_MAX_STEPS);
        move_remain_step = STEP_MOTOR_HOME_DIR * STEP_MOTOR_TRAVEL_MAX_STEPS;
        (void)appEventClear(APP_EVT_STEP_MOTOR_DONE);
        osDelay(STEP_MOTOR_IDLE_MS);
        continue;
      }

      if((active_msg.cmd == RTOS_STEP_MOTOR_CMD_CALIBRATION) &&
         (calibration_phase == STEP_MOTOR_CALIBRATION_PHASE_TO_HOME))
      {
        // Calibration은 Home 센서를 찾은 뒤 준비 위치로 빠지기 위해 End 방향으로 500step 더 이동한다.
        target_evt = 0U;
        calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_READY_OFFSET;
        use_sensor_profile = false;
        move_pulse_delay_us = STEP_MOTOR_CALIBRATION_DELAY_US;
        move_done_step = 0U;
        move_profile_step = STEP_MOTOR_CALIBRATION_READY_OFFSET_STEPS;
        move_remain_step = STEP_MOTOR_END_DIR * (int32_t)STEP_MOTOR_CALIBRATION_READY_OFFSET_STEPS;
        (void)appEventClear(APP_EVT_STEP_MOTOR_DONE);
        osDelay(STEP_MOTOR_IDLE_MS);
        continue;
      }

      move_remain_step = 0;
      taskStepMotorStopCurrent(move_ch);
      taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
      calibration_phase = STEP_MOTOR_CALIBRATION_PHASE_NONE;
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

      if(is_target_move == true)
      {
        if(is_async_chunk_running == true)
        {
          uint32_t evt_flags;

          // PWM count 완료 또는 목표 센서 감지 이벤트가 올 때까지 task를 잠시 block한다.
          evt_flags = appEventWait(APP_EVT_STEP_MOTOR_DONE | target_evt,
                                   osFlagsWaitAny | osFlagsNoClear,
                                   STEP_MOTOR_IDLE_MS);

          if((evt_flags & osFlagsError) != 0U)
          {
            osDelay(STEP_MOTOR_IDLE_MS);
          }
          else if((evt_flags & APP_EVT_STEP_MOTOR_DONE) != 0U)
          {
            (void)appEventClear(APP_EVT_STEP_MOTOR_DONE);

            // async chunk가 정상 완료됐으므로 남은 이동량과 프로파일 진행량을 갱신한다.
            move_remain_step -= async_chunk_step;
            move_done_step += taskStepMotorAbsStep(async_chunk_step);
            is_async_chunk_running = false;
            async_chunk_step = 0;

            if(move_remain_step == 0)
            {
              if((active_msg.cmd == RTOS_STEP_MOTOR_CMD_CALIBRATION) &&
                 (calibration_phase == STEP_MOTOR_CALIBRATION_PHASE_READY_OFFSET))
              {
                // Calibration 마지막 offset 이동은 센서 감지가 아니라 500step 완료가 정상 종료 조건이다.
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
            // 기존 MoveToHome/MoveToEnd는 가속/중간/빠른/감속 프로파일에 따라 속도와 chunk 크기를 정한다.
            pulse_delay_us = taskStepMotorGetSensorMoveDelay(move_done_step, move_profile_step, move_pulse_delay_us);
            chunk_step = taskStepMotorGetSensorMoveChunk(move_done_step,
                                                         move_profile_step,
                                                         remain_abs_step);
          }
          else
          {
            // Calibration은 프로파일을 쓰지 않고 고정 속도로 일정한 chunk만큼 이동한다.
            pulse_delay_us = move_pulse_delay_us;
            chunk_step = STEP_MOTOR_CALIBRATION_CHUNK_STEPS;

            if(chunk_step > remain_abs_step)
            {
              chunk_step = remain_abs_step;
            }
          }

          step = (move_remain_step > 0) ? (int32_t)chunk_step : -(int32_t)chunk_step;

          // 새 chunk 시작 전에 이전 완료 이벤트가 남아 있지 않게 한다.
          (void)appEventClear(APP_EVT_STEP_MOTOR_DONE);

          if(dm542MoveStepAsync(move_ch, step, pulse_delay_us) == true)
          {
            // Home/End 센서를 찾는 phase에서만 SN04 EXTI가 DM542 PWM을 즉시 끊을 수 있게 허용한다.
            (void)dm542EnableSensorStop(move_ch, target_evt != 0U);
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
        // 일반 MoveStep 명령은 기존 blocking 1-step 이동 방식을 유지한다.
        step = (move_remain_step > 0) ? 1 : -1;

        if(dm542MoveStep(move_ch, step, pulse_delay_us) == true)
        {
          move_remain_step -= step;
          move_done_step++;

          if(move_remain_step == 0)
          {
            taskStepMotorSendAck(&active_msg, RTOS_STEP_MOTOR_ACK_DONE);
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

    if(done_step < accel_half_step)
    {
      return STEP_MOTOR_SENSOR_ACCEL_DELAY_US;
    }

    if(done_step < accel_step)
    {
      return STEP_MOTOR_SENSOR_MID_DELAY_US;
    }

    if((done_step - accel_step) < decel_half_step)
    {
      return STEP_MOTOR_SENSOR_MID_DELAY_US;
    }

    return STEP_MOTOR_SENSOR_DECEL_DELAY_US;
  }

  slow_start_step = profile_step - STEP_MOTOR_SENSOR_SLOW_STEPS;

  if(done_step < STEP_MOTOR_SENSOR_SLOW_STEPS)
  {
    if((slow_half_step == 0U) || (done_step < slow_half_step))
    {
      return STEP_MOTOR_SENSOR_ACCEL_DELAY_US;
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

    return STEP_MOTOR_SENSOR_DECEL_DELAY_US;
  }

  return default_delay_us;
}

static uint32_t taskStepMotorGetSensorMoveChunk(uint32_t done_step, uint32_t profile_step, uint32_t remain_step)
{
  uint32_t chunk_step = STEP_MOTOR_SENSOR_DECEL_CHUNK_STEPS;
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
      chunk_step = STEP_MOTOR_SENSOR_ACCEL_CHUNK_STEPS;
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
#endif
