/*
 * dm542.c
 *
 *  Created on: May 16, 2026
 *      Author: young
 */


#include "DM542/dm542.h"
#include "pwm.h"
#include "gpio.h"
#include "cli.h"

#ifdef _USE_DM542

#ifdef _USE_HW_CLI
static void cliDm542(cli_args_t *args);
#endif

#ifndef DM542_STEP_PULSE_US
#define DM542_STEP_PULSE_US             10U
#endif

#ifndef DM542_STEP_PER_MM
#define DM542_STEP_PER_MM               1.0f
#endif

typedef struct
{
  bool is_open;
  bool is_busy;
  bool is_enabled;

  int32_t position_step;
  uint32_t remain_step;
} dm542_tbl_t;

static dm542_tbl_t dm542_tbl[DM542_MAX_CH] =
{
  {
    .is_open        = false,
    .is_busy        = false,
    .is_enabled     = false,

    .position_step  = 0,
    .remain_step    = 0,
  },
};

bool dm542Init(void)
{
  bool ret = true;

  for(uint8_t i = 0; i < DM542_MAX_CH; i++)
  {
    dm542_tbl[i].is_open       = false;
    dm542_tbl[i].is_busy       = false;
    dm542_tbl[i].is_enabled    = false;
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
  if(ch >= DM542_MAX_CH) 					return false;
  if(dm542_tbl[ch].is_open == true)			return true;

  if(pwmOpen(DM542_PUL) != true)
  {
    return false;
  }

  dm542_tbl[ch].is_busy       = false;
  dm542_tbl[ch].is_enabled    = false;
  dm542_tbl[ch].position_step = 0;
  dm542_tbl[ch].remain_step   = 0;
  dm542_tbl[ch].is_open       = true;

  return true;
}

bool dm542IsOpen(uint8_t ch)
{
  if(ch >= DM542_MAX_CH)  return false;

  return dm542_tbl[ch].is_open;
}

bool dm542IsBusy(uint8_t ch)
{
  if(ch >= DM542_MAX_CH)  return false;

  return dm542_tbl[ch].is_busy;
}

bool dm542IsEnabled(uint8_t ch)
{
  if(ch >= DM542_MAX_CH)  return false;

  return dm542_tbl[ch].is_enabled;
}

bool dm542Enable(uint8_t ch)
{
  if(ch >= DM542_MAX_CH) return false;
  if(dm542_tbl[ch].is_open != true) return false;

  dm542_tbl[ch].is_enabled = true;

  return true;
}

bool dm542Disable(uint8_t ch)
{
  if(ch >= DM542_MAX_CH) return false;
  if(dm542_tbl[ch].is_open != true) return false;

  dm542_tbl[ch].is_enabled = false;

  return true;
}

bool dm542Start(uint8_t ch)
{
  if(ch >= DM542_MAX_CH) return false;
  if(dm542_tbl[ch].is_open != true) return false;
  if(dm542_tbl[ch].is_enabled != true) return false;

  if(pwmStart(DM542_PUL) != true)
  {
    return false;
  }

  dm542_tbl[ch].is_busy = true;

  return true;
}

bool dm542Stop(uint8_t ch)
{
  if(ch >= DM542_MAX_CH) return false;
  if(dm542_tbl[ch].is_open != true) return false;

  if(pwmStop(DM542_PUL) != true)
  {
    return false;
  }

  dm542_tbl[ch].is_busy = false;

  return true;
}

bool dm542Step(uint8_t ch)
{
  bool ret;

  if(ch >= DM542_MAX_CH) return false;
  if(dm542_tbl[ch].is_open != true) return false;
  if(dm542_tbl[ch].is_enabled != true) return false;
  if(dm542_tbl[ch].is_busy == true) return false;

  dm542_tbl[ch].is_busy     = true;
  dm542_tbl[ch].remain_step = 1;

  ret = pwmRunUs(DM542_PUL, DM542_STEP_PULSE_US);
  if(ret == true)
  {
    dm542_tbl[ch].position_step++;
    dm542_tbl[ch].remain_step = 0;
  }

  dm542_tbl[ch].is_busy = false;

  return ret;
}

bool dm542SetPrescaler(uint8_t ch, uint32_t prescaler)
{
  if(ch >= DM542_MAX_CH) return false;
  if(dm542_tbl[ch].is_open != true) return false;

  return pwmSetPrescaler(DM542_PUL, prescaler);
}

bool dm542SetPeriod(uint8_t ch, uint32_t period)
{
  if(ch >= DM542_MAX_CH) return false;
  if(dm542_tbl[ch].is_open != true) return false;

  return pwmSetPeriod(DM542_PUL, period);
}

bool dm542SetPulse(uint8_t ch, uint32_t pulse)
{
  if(ch >= DM542_MAX_CH) return false;
  if(dm542_tbl[ch].is_open != true) return false;

  return pwmSetPulse(DM542_PUL, pulse);
}

bool dm542SetFreq(uint8_t ch, uint32_t freq_hz)
{
  uint32_t timer_clk;
  uint32_t prescaler;
  uint32_t period;
  uint32_t pulse;

  if(ch >= DM542_MAX_CH) return false;
  if(dm542_tbl[ch].is_open != true) return false;
  if(freq_hz == 0U) return false;

  timer_clk = HAL_RCC_GetHCLKFreq();
  prescaler = timer_clk / 1000000U;
  if(prescaler == 0U) return false;
  prescaler--;

  period = 1000000U / freq_hz;
  if(period == 0U) return false;
  period--;

  pulse = (period + 1U) / 2U;
  if(pulse > 0U)
  {
    pulse--;
  }

  if(dm542SetPrescaler(ch, prescaler) != true) return false;
  if(dm542SetPeriod(ch, period) != true) return false;
  if(dm542SetPulse(ch, pulse) != true) return false;

  return true;
}

bool dm542MoveStep(uint8_t ch, int32_t step, uint32_t pulse_delay_us)
{
  bool dir;
  uint32_t step_count;

  if(ch >= DM542_MAX_CH) return false;
  if(dm542_tbl[ch].is_open != true) return false;
  if(dm542_tbl[ch].is_enabled != true) return false;
  if(dm542_tbl[ch].is_busy == true) return false;
  if(pulse_delay_us == 0U) return false;

  if(step == 0)
  {
    return true;
  }

  dir = step > 0;
  if(step > 0)
  {
    step_count = (uint32_t)step;
  }
  else
  {
    step_count = (uint32_t)(-(step + 1)) + 1U;
  }

  gpioPinWrite(DM542_DIR, dir);

  dm542_tbl[ch].is_busy     = true;
  dm542_tbl[ch].remain_step = step_count;

  while(dm542_tbl[ch].remain_step > 0U)
  {
    if(pwmRunUs(DM542_PUL, pulse_delay_us) != true)
    {
      dm542_tbl[ch].is_busy = false;
      return false;
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
  }

  dm542_tbl[ch].is_busy = false;

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

#ifdef _USE_HW_CLI
static void cliDm542(cli_args_t *args)
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
      for(uint8_t i = 0; i < DM542_MAX_CH; i++)
      {
        cliPrintf("dm542 %d open:%d busy:%d enable:%d pos:%ld remain:%lu\n",
                  i,
                  dm542_tbl[i].is_open,
                  dm542_tbl[i].is_busy,
                  dm542_tbl[i].is_enabled,
                  (long)dm542_tbl[i].position_step,
                  dm542_tbl[i].remain_step);
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

    if(args->isStr(0, "enable") == true)
    {
      cmd_ret = dm542Enable(ch);
      cliPrintf("dm542 enable %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "disable") == true)
    {
      cmd_ret = dm542Disable(ch);
      cliPrintf("dm542 disable %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "step") == true)
    {
      cmd_ret = dm542Step(ch);
      cliPrintf("dm542 step %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
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

  if(args->argc == 4)
  {
    ch    = (uint8_t)args->getData(1);
    step  = args->getData(2);
    value = (uint32_t)args->getData(3);

    if(args->isStr(0, "move") == true)
    {
      cmd_ret = dm542MoveStep(ch, step, value);
      cliPrintf("dm542 move %d %ld %luus : %s\n",
                ch,
                (long)step,
                value,
                cmd_ret ? "OK" : "FAIL");
      ret = true;
    }
  }

  if(ret != true)
  {
    cliPrintf("dm542 show\n");
    cliPrintf("dm542 open ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 enable ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 disable ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 step ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 start ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 stop ch[0~%d]\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 freq ch[0~%d] hz\n", DM542_MAX_CH - 1);
    cliPrintf("dm542 move ch[0~%d] step pulse_delay_us\n", DM542_MAX_CH - 1);
  }
}
#endif

#endif
