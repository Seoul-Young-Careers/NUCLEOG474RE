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

  int32_t position_step;
  uint32_t remain_step;
} dm542_tbl_t;

static dm542_tbl_t dm542_tbl[DM542_MAX_CH] =
{
  {
    .is_open        = false,
    .is_busy        = false,

    .position_step  = 0,
    .remain_step    = 0,
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
    dm542_tbl[i].is_open       = false;
    dm542_tbl[i].is_busy       = false;
    dm542_tbl[i].position_step = 0;
    dm542_tbl[i].remain_step   = 0;

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

    dm542_tbl[ch].is_busy       = false;
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

    if(pwmStop(DM542_PUL) != true) break;

    dm542_tbl[ch].remain_step = 0U;
    dm542_tbl[ch].is_busy     = false;
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
    dm542_tbl[ch].remain_step = 0U;
    dm542_tbl[ch].is_busy     = false;
  }

  dm542Unlock();
}

#ifdef _USE_HW_CLI
static void cliDm542(cli_args_t *args)
{
  bool ret = false;
  bool cmd_ret;
  uint8_t ch;
  uint32_t value;

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
  }

  if(ret != true)
  {
    cliPrintf("dm542 show\n");
    cliPrintf("dm542 open ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 start ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 stop ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 freq ch[0~%d] hz\n", DM542_MAX_CH - 1);
  }
}
#endif

#endif
