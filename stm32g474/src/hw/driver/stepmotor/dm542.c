/*
 * dm542.c
 *
 *  Created on: May 16, 2026
 *      Author: young
 */


#include <stepmotor/dm542.h>
#include "pwm.h"
#include "gpio.h"
#include "cli.h"

#ifdef _USE_DM542

#ifdef _USE_HW_CLI
static void cliDm542(cli_args_t *args);
#endif

#ifndef DM542_STEP_PER_MM
#define DM542_STEP_PER_MM               1.0f
#endif

typedef struct
{
  bool is_open;
  bool is_busy;
  // Timer PWM으로 동작 중인 비동기 이동 상태인지 표시한다.
  bool is_async_move;
  // 비동기 이동 중 position_step을 갱신하기 위해 방향을 저장한다.
  bool async_dir;
  // Home/End 같은 센서 타겟 이동 중일 때만 EXTI 정지를 허용한다.
  bool sensor_stop_enabled;

  int32_t position_step;
  uint32_t remain_step;
  // 비동기 이동이 정상 완료됐을 때 step motor task로 알릴 콜백이다.
  dm542_done_callback_t done_callback;
} dm542_tbl_t;

static dm542_tbl_t dm542_tbl[DM542_MAX_CH] =
{
  {
    .is_open        = false,
    .is_busy        = false,
    .is_async_move  = false,
    .async_dir      = false,
    .sensor_stop_enabled = false,

    .position_step  = 0,
    .remain_step    = 0,
    .done_callback  = NULL,
  },
};

#ifdef _USE_HW_RTOS
static osMutexId_t dm542_mutex = NULL;
static const osMutexAttr_t dm542_mutex_attr =
{
  .name      = "dm542",
  .attr_bits = osMutexRecursive | osMutexPrioInherit,
};
#endif

static bool dm542Lock(void);
static void dm542Unlock(void);
static void dm542EndMove(uint8_t ch);
// int32_t 최소값까지 안전하게 step 절댓값을 구한다.
static uint32_t dm542AbsStep(int32_t step);
// pulse_delay_us를 TIM PWM prescaler/period/pulse 설정으로 변환한다.
static bool dm542ConfigurePulseDelayUs(uint32_t pulse_delay_us);
// 실제 출력된 pulse 수를 position_step에 반영한다.
static void dm542ApplyMovedStep(uint8_t ch, uint32_t moved_step, bool dir);
// PWM count 출력이 끝났을 때 호출되는 내부 완료 콜백이다.
static void dm542PwmDoneCallback(uint8_t pwm_ch);

bool dm542Init(void)
{
  bool ret = true;

#ifdef _USE_HW_RTOS
  if(dm542_mutex == NULL)
  {
    dm542_mutex = osMutexNew(&dm542_mutex_attr);
    if(dm542_mutex == NULL)
    {
      ret = false;
    }
  }
#endif

  for(uint8_t i = 0; i < DM542_MAX_CH; i++)
  {
    // 초기화 시 비동기 이동 관련 상태도 모두 기본값으로 되돌린다.
    dm542_tbl[i].is_open       = false;
    dm542_tbl[i].is_busy       = false;
    dm542_tbl[i].is_async_move = false;
    dm542_tbl[i].async_dir     = false;
    dm542_tbl[i].sensor_stop_enabled = false;
    dm542_tbl[i].position_step = 0;
    dm542_tbl[i].remain_step   = 0;
    dm542_tbl[i].done_callback = NULL;

    if(dm542Open(i) != true)
    {
      ret = false;
    }
  }

#ifdef _USE_HW_CLI
  cliAdd("dm542", cliDm542);
#endif

  return ret;
}

bool dm542Open(uint8_t ch)
{
  bool ret = false;

  if(dm542Lock() != true) return false;

  do
  {
    if(ch >= DM542_MAX_CH) break;
    if(dm542_tbl[ch].is_open == true)
    {
      ret = true;
      break;
    }

    if(pwmOpen(DM542_PUL) != true) break;
    // DM542 STEP PWM이 지정 count를 모두 출력하면 dm542PwmDoneCallback()이 호출된다.
    if(pwmAttachDoneCallback(DM542_PUL, dm542PwmDoneCallback) != true) break;

    // 채널 open 시 이전 이동 상태가 남지 않도록 정리한다.
    dm542_tbl[ch].is_busy       = false;
    dm542_tbl[ch].is_async_move = false;
    dm542_tbl[ch].async_dir     = false;
    dm542_tbl[ch].sensor_stop_enabled = false;
    dm542_tbl[ch].position_step = 0;
    dm542_tbl[ch].remain_step   = 0;
    dm542_tbl[ch].is_open       = true;

    ret = true;
  } while(0);

  dm542Unlock();

  return ret;
}

bool dm542IsOpen(uint8_t ch)
{
  bool ret = false;

  if(dm542Lock() != true) return false;

  if(ch < DM542_MAX_CH)
  {
    ret = dm542_tbl[ch].is_open;
  }

  dm542Unlock();

  return ret;
}

bool dm542IsBusy(uint8_t ch)
{
  bool ret = false;

  if(dm542Lock() != true) return false;

  if(ch < DM542_MAX_CH)
  {
    ret = dm542_tbl[ch].is_busy;
  }

  dm542Unlock();

  return ret;
}

bool dm542Start(uint8_t ch)
{
  bool ret = false;

  if(dm542Lock() != true) return false;

  do
  {
    if(ch >= DM542_MAX_CH) break;
    if(dm542_tbl[ch].is_open != true) break;
    if(dm542_tbl[ch].is_busy == true) break;

    if(pwmStart(DM542_PUL) != true) break;

    dm542_tbl[ch].is_busy = true;
    ret = true;
  } while(0);

  dm542Unlock();

  return ret;
}

bool dm542Stop(uint8_t ch)
{
  bool ret = false;

  if(dm542Lock() != true) return false;

  do
  {
    if(ch >= DM542_MAX_CH) break;
    if(dm542_tbl[ch].is_open != true) break;

    if((dm542_tbl[ch].is_busy == true) && (dm542_tbl[ch].is_async_move == true))
    {
      uint32_t pwm_remain = pwmGetRemainCount(DM542_PUL);
      uint32_t moved_step = 0U;

      // 비동기 이동 중 정지되면 남은 PWM count로 실제 출력된 step 수를 계산한다.
      if(dm542_tbl[ch].remain_step > pwm_remain)
      {
        moved_step = dm542_tbl[ch].remain_step - pwm_remain;
      }

      dm542ApplyMovedStep(ch, moved_step, dm542_tbl[ch].async_dir);
    }

    if(pwmStop(DM542_PUL) != true) break;

    // 정지 후에는 비동기 이동과 센서 정지 허용 상태를 모두 해제한다.
    dm542_tbl[ch].remain_step         = 0U;
    dm542_tbl[ch].is_busy             = false;
    dm542_tbl[ch].is_async_move       = false;
    dm542_tbl[ch].sensor_stop_enabled = false;
    ret = true;
  } while(0);

  dm542Unlock();

  return ret;
}

bool dm542SetPrescaler(uint8_t ch, uint32_t prescaler)
{
  bool ret = false;

  if(dm542Lock() != true) return false;

  do
  {
    if(ch >= DM542_MAX_CH) break;
    if(dm542_tbl[ch].is_open != true) break;
    if(dm542_tbl[ch].is_busy == true) break;

    ret = pwmSetPrescaler(DM542_PUL, prescaler);
  } while(0);

  dm542Unlock();

  return ret;
}

bool dm542SetPeriod(uint8_t ch, uint32_t period)
{
  bool ret = false;

  if(dm542Lock() != true) return false;

  do
  {
    if(ch >= DM542_MAX_CH) break;
    if(dm542_tbl[ch].is_open != true) break;
    if(dm542_tbl[ch].is_busy == true) break;

    ret = pwmSetPeriod(DM542_PUL, period);
  } while(0);

  dm542Unlock();

  return ret;
}

bool dm542SetPulse(uint8_t ch, uint32_t pulse)
{
  bool ret = false;

  if(dm542Lock() != true) return false;

  do
  {
    if(ch >= DM542_MAX_CH) break;
    if(dm542_tbl[ch].is_open != true) break;
    if(dm542_tbl[ch].is_busy == true) break;

    ret = pwmSetPulse(DM542_PUL, pulse);
  } while(0);

  dm542Unlock();

  return ret;
}

bool dm542SetFreq(uint8_t ch, uint32_t freq_hz)
{
  bool ret = false;
  uint32_t timer_clk;
  uint32_t prescaler;
  uint32_t period;
  uint32_t pulse;

  if(dm542Lock() != true) return false;

  do
  {
    if(ch >= DM542_MAX_CH) break;
    if(dm542_tbl[ch].is_open != true) break;
    if(dm542_tbl[ch].is_busy == true) break;
    if(freq_hz == 0U) break;

    timer_clk = HAL_RCC_GetHCLKFreq();
    prescaler = timer_clk / 1000000U;
    if(prescaler == 0U) break;
    prescaler--;

    period = 1000000U / freq_hz;
    if(period == 0U) break;
    period--;

    pulse = (period + 1U) / 2U;
    if(pulse > 0U)
    {
      pulse--;
    }

    if(dm542SetPrescaler(ch, prescaler) != true) break;
    if(dm542SetPeriod(ch, period) != true) break;
    if(dm542SetPulse(ch, pulse) != true) break;

    ret = true;
  } while(0);

  dm542Unlock();

  return ret;
}

bool dm542MoveStep(uint8_t ch, int32_t step, uint32_t pulse_delay_us)
{
  bool ret = false;
  bool dir;
  uint32_t step_count;

  if(pulse_delay_us == 0U) return false;

  dir = step > 0;
  if(step > 0)
  {
    step_count = (uint32_t)step;
  }
  else
  {
    step_count = (uint32_t)(-(step + 1)) + 1U;
  }

  if(dm542Lock() != true) return false;

  do
  {
    if(ch >= DM542_MAX_CH) break;
    if(dm542_tbl[ch].is_open != true) break;
    if(dm542_tbl[ch].is_busy == true) break;

    if(step == 0)
    {
      ret = true;
      break;
    }

    gpioPinWrite(DM542_DIR, dir);

    dm542_tbl[ch].is_busy     = true;
    dm542_tbl[ch].remain_step = step_count;
    ret = true;
  } while(0);

  dm542Unlock();

  if(ret != true)
  {
    return false;
  }

  while(true)
  {
    if(dm542Lock() != true)
    {
      dm542EndMove(ch);
      return false;
    }

    if(dm542_tbl[ch].remain_step == 0U)
    {
      dm542_tbl[ch].is_busy = false;
      dm542Unlock();
      break;
    }

    dm542Unlock();

    if(pwmRunUs(DM542_PUL, pulse_delay_us) != true)
    {
      dm542EndMove(ch);
      return false;
    }

    if(dm542Lock() != true)
    {
      dm542EndMove(ch);
      return false;
    }

    if(dm542_tbl[ch].remain_step == 0U)
    {
      dm542_tbl[ch].is_busy = false;
      dm542Unlock();
      break;
    }

    if(dir == true)
    {
      dm542_tbl[ch].position_step++;
    }
    else
    {
      dm542_tbl[ch].position_step--;
    }

    dm542_tbl[ch].remain_step--;

    if(dm542_tbl[ch].remain_step == 0U)
    {
      dm542_tbl[ch].is_busy = false;
    }

    dm542Unlock();
  }

  return true;
}

bool dm542MoveStepAsync(uint8_t ch, int32_t step, uint32_t pulse_delay_us)
{
  bool ret = false;
  bool dir;
  uint32_t step_count;

  if(pulse_delay_us == 0U) return false;

  // step 부호는 방향으로, 절댓값은 출력할 STEP pulse 개수로 사용한다.
  dir = step > 0;
  step_count = dm542AbsStep(step);

  if(dm542Lock() != true) return false;

  do
  {
    if(ch >= DM542_MAX_CH) break;
    if(dm542_tbl[ch].is_open != true) break;
    if(dm542_tbl[ch].is_busy == true) break;

    if(step_count == 0U)
    {
      // 0 step 명령은 실제 PWM 출력 없이 성공 처리한다.
      ret = true;
      break;
    }

    // pulse_delay_us 기준으로 TIM PWM 주기와 duty를 설정한다.
    if(dm542ConfigurePulseDelayUs(pulse_delay_us) != true) break;

    gpioPinWrite(DM542_DIR, dir);

    // PWM 완료/중간 정지 시 위치 계산에 필요한 상태를 저장한다.
    dm542_tbl[ch].is_busy             = true;
    dm542_tbl[ch].is_async_move       = true;
    dm542_tbl[ch].async_dir           = dir;
    dm542_tbl[ch].sensor_stop_enabled = false;
    dm542_tbl[ch].remain_step         = step_count;

    // 지정한 step_count만큼 STEP pulse를 Timer가 자동 출력하도록 시작한다.
    if(pwmStartCount(DM542_PUL, step_count) != true)
    {
      // PWM 시작 실패 시 busy 상태가 남지 않도록 원복한다.
      dm542_tbl[ch].is_busy       = false;
      dm542_tbl[ch].is_async_move = false;
      dm542_tbl[ch].remain_step   = 0U;
      break;
    }

    ret = true;
  } while(0);

  dm542Unlock();

  return ret;
}

bool dm542AttachDoneCallback(uint8_t ch, dm542_done_callback_t callback)
{
  bool ret = false;

  if(dm542Lock() != true) return false;

  if(ch < DM542_MAX_CH)
  {
    // async 이동 완료를 task 계층으로 전달할 콜백을 저장한다.
    dm542_tbl[ch].done_callback = callback;
    ret = true;
  }

  dm542Unlock();

  return ret;
}

bool dm542EnableSensorStop(uint8_t ch, bool enable)
{
  bool ret = false;

  if(dm542Lock() != true) return false;

  if(ch < DM542_MAX_CH)
  {
    // Home/End 이동 중에만 센서 EXTI가 PWM을 즉시 끊을 수 있게 한다.
    dm542_tbl[ch].sensor_stop_enabled = enable;
    ret = true;
  }

  dm542Unlock();

  return ret;
}

bool dm542StopFromISR(uint8_t ch)
{
  uint32_t pwm_remain;
  uint32_t moved_step = 0U;

  if(ch >= DM542_MAX_CH) return false;
  if(dm542_tbl[ch].is_open != true) return false;

  if(dm542_tbl[ch].is_busy != true)
  {
    // 이미 멈춘 상태라면 남아 있을 수 있는 async 플래그만 정리한다.
    dm542_tbl[ch].remain_step         = 0U;
    dm542_tbl[ch].is_async_move       = false;
    dm542_tbl[ch].sensor_stop_enabled = false;
    return true;
  }

  if((dm542_tbl[ch].is_busy == true) && (dm542_tbl[ch].is_async_move == true))
  {
    pwm_remain = pwmGetRemainCount(DM542_PUL);
    // ISR 정지 시점까지 실제로 출력된 pulse 수만 위치에 반영한다.
    if(dm542_tbl[ch].remain_step > pwm_remain)
    {
      moved_step = dm542_tbl[ch].remain_step - pwm_remain;
    }

    dm542ApplyMovedStep(ch, moved_step, dm542_tbl[ch].async_dir);
  }

  if(pwmStopFromISR(DM542_PUL) != true) return false;

  // ISR 정지 후에는 task가 다시 명령을 줄 수 있도록 상태를 비운다.
  dm542_tbl[ch].remain_step         = 0U;
  dm542_tbl[ch].is_busy             = false;
  dm542_tbl[ch].is_async_move       = false;
  dm542_tbl[ch].sensor_stop_enabled = false;

  return true;
}

bool dm542StopBySensorFromISR(uint8_t ch)
{
  // SN04가 감지되면 이동 종류와 상관없이 즉시 STEP PWM을 정지한다.
  return dm542StopFromISR(ch);
}

bool dm542MoveMm(uint8_t ch, float mm, uint32_t pulse_delay_us)
{
  float step_f;
  int32_t step;

  if(DM542_STEP_PER_MM <= 0.0f) return false;

  step_f = mm * DM542_STEP_PER_MM;
  if(step_f >= 0.0f)
  {
    step = (int32_t)(step_f + 0.5f);
  }
  else
  {
    step = (int32_t)(step_f - 0.5f);
  }

  return dm542MoveStep(ch, step, pulse_delay_us);
}

bool dm542ReadData(uint8_t ch, dm542_data_t *p_data)
{
  bool ret = false;

  if(dm542Lock() != true) return false;

  do
  {
    if(ch >= DM542_MAX_CH) break;
    if(p_data == NULL) break;

    p_data->is_open       = dm542_tbl[ch].is_open;
    p_data->is_busy       = dm542_tbl[ch].is_busy;
    p_data->position_step = dm542_tbl[ch].position_step;
    p_data->remain_step   = dm542_tbl[ch].remain_step;

    ret = true;
  } while(0);

  dm542Unlock();

  return ret;
}

bool dm542SetPositionStep(uint8_t ch, int32_t position_step)
{
  bool ret = false;

  if(dm542Lock() != true) return false;

  do
  {
    if(ch >= DM542_MAX_CH) break;
    if(dm542_tbl[ch].is_open != true) break;
    if(dm542_tbl[ch].is_busy == true) break;

    dm542_tbl[ch].position_step = position_step;
    ret = true;
  } while(0);

  dm542Unlock();

  return ret;
}

static bool dm542Lock(void)
{
#ifdef _USE_HW_RTOS
  if(__get_IPSR() != 0U) return false;

  if((dm542_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    return osMutexAcquire(dm542_mutex, DM542_LOCK_TIMEOUT_MS) == osOK;
  }
#endif

  return true;
}

static void dm542Unlock(void)
{
#ifdef _USE_HW_RTOS
  if((dm542_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    (void)osMutexRelease(dm542_mutex);
  }
#endif
}

static void dm542EndMove(uint8_t ch)
{
  if(dm542Lock() != true) return;

  if(ch < DM542_MAX_CH)
  {
    // 에러 종료 시 async 관련 상태가 다음 명령에 영향을 주지 않도록 정리한다.
    dm542_tbl[ch].remain_step         = 0U;
    dm542_tbl[ch].is_busy             = false;
    dm542_tbl[ch].is_async_move       = false;
    dm542_tbl[ch].sensor_stop_enabled = false;
  }

  dm542Unlock();
}

static uint32_t dm542AbsStep(int32_t step)
{
  if(step >= 0)
  {
    return (uint32_t)step;
  }

  // INT32_MIN을 바로 음수 변환하면 overflow가 날 수 있어 한 칸 나눠 계산한다.
  return (uint32_t)(-(step + 1)) + 1U;
}

static bool dm542ConfigurePulseDelayUs(uint32_t pulse_delay_us)
{
  uint32_t timer_clk;
  uint32_t prescaler;
  uint32_t period;
  uint32_t pulse;

  if(pulse_delay_us == 0U) return false;

  // Timer를 1MHz tick으로 맞춰 pulse_delay_us를 곧 PWM period로 사용한다.
  timer_clk = HAL_RCC_GetHCLKFreq();
  prescaler = timer_clk / 1000000U;
  if(prescaler == 0U) return false;
  prescaler--;

  period = pulse_delay_us;
  if(period == 0U) return false;
  period--;

  // STEP pulse는 한 주기의 절반 정도 HIGH가 되도록 설정한다.
  pulse = (period + 1U) / 2U;
  if(pulse > 0U)
  {
    pulse--;
  }

  if(pwmSetPrescaler(DM542_PUL, prescaler) != true) return false;
  if(pwmSetPeriod(DM542_PUL, period) != true) return false;
  if(pwmSetPulse(DM542_PUL, pulse) != true) return false;

  return true;
}

static void dm542ApplyMovedStep(uint8_t ch, uint32_t moved_step, bool dir)
{
  if(ch >= DM542_MAX_CH) return;

  // 출력된 STEP pulse 수만큼 현재 위치 카운터를 방향에 맞춰 갱신한다.
  if(dir == true)
  {
    dm542_tbl[ch].position_step += (int32_t)moved_step;
  }
  else
  {
    dm542_tbl[ch].position_step -= (int32_t)moved_step;
  }
}

static void dm542PwmDoneCallback(uint8_t pwm_ch)
{
  uint8_t ch = _DEF_DM542_1;
  dm542_done_callback_t done_callback = NULL;

  if(pwm_ch != DM542_PUL) return;
  if(ch >= DM542_MAX_CH) return;
  if(dm542_tbl[ch].is_async_move != true) return;

  // count 출력이 정상 완료된 경우에는 남은 step 전체가 이동됐다고 본다.
  dm542ApplyMovedStep(ch, dm542_tbl[ch].remain_step, dm542_tbl[ch].async_dir);

  // 완료 후 상태를 정리하고 센서 정지 허용도 해제한다.
  dm542_tbl[ch].remain_step         = 0U;
  dm542_tbl[ch].is_busy             = false;
  dm542_tbl[ch].is_async_move       = false;
  dm542_tbl[ch].sensor_stop_enabled = false;

  done_callback = dm542_tbl[ch].done_callback;
  if(done_callback != NULL)
  {
    // 상위 task에 "비동기 이동 완료" 이벤트를 전달한다.
    done_callback(ch);
  }
}

#ifdef _USE_HW_CLI
static void cliDm542(cli_args_t *args)
{
  bool ret = false;
  bool cmd_ret;
  uint8_t ch;
  uint32_t value;
  int32_t step;
  uint32_t delay_us;

  if(args->argc == 1)
  {
    if(args->isStr(0, "show") == true)
    {
      for(uint8_t i = 0; i < DM542_MAX_CH; i++)
      {
        dm542_data_t data;

        if(dm542ReadData(i, &data) == true)
        {
          cliPrintf("dm542 %d open:%d busy:%d pos:%ld remain:%lu\n",
                    i,
                    data.is_open,
                    data.is_busy,
                    (long)data.position_step,
                    data.remain_step);
        }
      }

      ret = true;
    }
  }

  if(args->argc == 2)
  {
    ch = (uint8_t)args->getData(1);

    if(args->isStr(0, "open") == true)
    {
      cmd_ret = dm542Open(ch);
      cliPrintf("dm542 open %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "start") == true)
    {
      cmd_ret = dm542Start(ch);
      cliPrintf("dm542 start %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "stop") == true)
    {
      cmd_ret = dm542Stop(ch);
      cliPrintf("dm542 stop %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "read") == true)
    {
      dm542_data_t data;

      cmd_ret = dm542ReadData(ch, &data);
      if(cmd_ret == true)
      {
        cliPrintf("dm542 read %d open:%d busy:%d pos:%ld remain:%lu\n",
                  ch,
                  data.is_open,
                  data.is_busy,
                  (long)data.position_step,
                  data.remain_step);
      }
      else
      {
        cliPrintf("dm542 read %d : FAIL\n", ch);
      }

      ret = true;
    }
  }

  if(args->argc == 3)
  {
    ch    = (uint8_t)args->getData(1);
    value = (uint32_t)args->getData(2);

    if(args->isStr(0, "freq") == true)
    {
      cmd_ret = dm542SetFreq(ch, value);
      cliPrintf("dm542 freq %d %luhz : %s\n", ch, value, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "prescaler") == true)
    {
      cmd_ret = dm542SetPrescaler(ch, value);
      cliPrintf("dm542 prescaler %d %lu : %s\n", ch, value, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "period") == true)
    {
      cmd_ret = dm542SetPeriod(ch, value);
      cliPrintf("dm542 period %d %lu : %s\n", ch, value, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "pulse") == true)
    {
      cmd_ret = dm542SetPulse(ch, value);
      cliPrintf("dm542 pulse %d %lu : %s\n", ch, value, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }
  }

  if(args->argc == 4)
  {
    ch       = (uint8_t)args->getData(1);
    step     = args->getData(2);
    delay_us = (uint32_t)args->getData(3);

    if(args->isStr(0, "move") == true)
    {
      cmd_ret = dm542MoveStep(ch, step, delay_us);
      cliPrintf("dm542 move %d %ld %luus : %s\n",
                ch,
                (long)step,
                delay_us,
                cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "mm") == true)
    {
      float mm = args->getFloat(2);

      cmd_ret = dm542MoveMm(ch, mm, delay_us);
      cliPrintf("dm542 mm %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }
  }

  if(ret != true)
  {
    cliPrintf("dm542 show\n");
    cliPrintf("dm542 open ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 read ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 start ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 stop ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 freq ch[0~%d] hz\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 prescaler ch[0~%d] value\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 period ch[0~%d] value\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 pulse ch[0~%d] value\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 move ch[0~%d] step delay_us\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 mm ch[0~%d] mm delay_us\n", DM542_MAX_CH - 1);
  }
}
#endif

#endif
