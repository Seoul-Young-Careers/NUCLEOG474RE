/*
 * bts7960.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "motordriver/bts7960.h"
#include "pwm.h"

#ifdef _USE_BTS7960

#ifdef _USE_HW_CLI
#include "cli.h"
static void cliBts7960(cli_args_t *args);
#endif

#define BTS7960_LPWM_CH                _DEF_PWM4
#define BTS7960_RPWM_CH                _DEF_PWM5

typedef struct
{
  bool is_open;
  bool is_started;

  bts7960_dir_t dir;
  int16_t speed;
  uint8_t duty;

  uint32_t prescaler;
  uint32_t period;
} bts7960_tbl_t;

static bts7960_cfg_t bts7960_cfg[BTS7960_MAX_CH] =
{
  {
    .rpwm_ch      = BTS7960_RPWM_CH,
    .lpwm_ch      = BTS7960_LPWM_CH,
    .freq_hz      = BTS7960_FREQ_HZ,
  },
};

static bts7960_tbl_t bts7960_tbl[BTS7960_MAX_CH];

#ifdef _USE_HW_RTOS
static osMutexId_t bts7960_mutex = NULL;
static const osMutexAttr_t bts7960_mutex_attr =
{
  .name      = "bts7960",
  .attr_bits = osMutexRecursive | osMutexPrioInherit,
};
#endif

static bool bts7960Lock(void);
static void bts7960Unlock(void);
static bool bts7960IsValidConfig(const bts7960_cfg_t *p_cfg);
static bool bts7960ApplyDuty(uint8_t ch, bts7960_dir_t dir, uint8_t duty);
static bool bts7960CalcTimer(uint32_t freq_hz, uint32_t *p_prescaler, uint32_t *p_period);
static uint32_t bts7960DutyToPulse(uint32_t period, uint8_t duty);

bool bts7960Init(void)
{
  bool ret = true;

#ifdef _USE_HW_RTOS
  if(bts7960_mutex == NULL)
  {
    bts7960_mutex = osMutexNew(&bts7960_mutex_attr);
    if(bts7960_mutex == NULL)
    {
      ret = false;
    }
  }
#endif

  for(uint8_t i = 0; i < BTS7960_MAX_CH; i++)
  {
    bts7960_tbl[i].is_open    = false;
    bts7960_tbl[i].is_started = false;
    bts7960_tbl[i].dir        = BTS7960_DIR_STOP;
    bts7960_tbl[i].speed      = 0;
    bts7960_tbl[i].duty       = 0U;
    bts7960_tbl[i].prescaler  = 0U;
    bts7960_tbl[i].period     = 0U;

    if(bts7960Open(i) != true)
    {
      ret = false;
    }
  }

#ifdef _USE_HW_CLI
  cliAdd("bts7960", cliBts7960);
#endif

  return ret;
}

bool bts7960Open(uint8_t ch)
{
  bool ret = false;

  if(bts7960Lock() != true) return false;

  do
  {
    if(ch >= BTS7960_MAX_CH) break;
    if(bts7960_tbl[ch].is_open == true)
    {
      ret = true;
      break;
    }
    if(bts7960IsValidConfig(&bts7960_cfg[ch]) != true) break;

    if(pwmOpen(bts7960_cfg[ch].rpwm_ch) != true) break;
    if(pwmOpen(bts7960_cfg[ch].lpwm_ch) != true) break;

    bts7960_tbl[ch].is_open = true;

    if(bts7960SetFreq(ch, bts7960_cfg[ch].freq_hz) != true)
    {
      bts7960_tbl[ch].is_open = false;
      break;
    }

    if(bts7960ApplyDuty(ch, BTS7960_DIR_STOP, 0U) != true)
    {
      bts7960_tbl[ch].is_open = false;
      break;
    }

    ret = true;
  } while(0);

  bts7960Unlock();

  return ret;
}

bool bts7960SetConfig(uint8_t ch, const bts7960_cfg_t *p_cfg)
{
  bool ret = false;
  bool was_open;

  if(bts7960Lock() != true) return false;

  do
  {
    if(ch >= BTS7960_MAX_CH) break;
    if(p_cfg == NULL) break;
    if(bts7960IsValidConfig(p_cfg) != true) break;
    if(bts7960_tbl[ch].is_started == true) break;

    was_open = bts7960_tbl[ch].is_open;

    bts7960_cfg[ch] = *p_cfg;
    bts7960_tbl[ch].is_open    = false;
    bts7960_tbl[ch].dir        = BTS7960_DIR_STOP;
    bts7960_tbl[ch].speed      = 0;
    bts7960_tbl[ch].duty       = 0U;

    if(was_open == true)
    {
      ret = bts7960Open(ch);
      break;
    }

    ret = true;
  } while(0);

  bts7960Unlock();

  return ret;
}

bool bts7960ReadConfig(uint8_t ch, bts7960_cfg_t *p_cfg)
{
  bool ret = false;

  if(bts7960Lock() != true) return false;

  do
  {
    if(ch >= BTS7960_MAX_CH) break;
    if(p_cfg == NULL) break;

    *p_cfg = bts7960_cfg[ch];

    ret = true;
  } while(0);

  bts7960Unlock();

  return ret;
}

bool bts7960IsOpen(uint8_t ch)
{
  bool ret = false;

  if(bts7960Lock() != true) return false;

  if(ch < BTS7960_MAX_CH)
  {
    ret = bts7960_tbl[ch].is_open;
  }

  bts7960Unlock();

  return ret;
}

bool bts7960IsStarted(uint8_t ch)
{
  bool ret = false;

  if(bts7960Lock() != true) return false;

  if(ch < BTS7960_MAX_CH)
  {
    ret = bts7960_tbl[ch].is_started;
  }

  bts7960Unlock();

  return ret;
}

bool bts7960Start(uint8_t ch)
{
  bool ret = false;

  if(bts7960Lock() != true) return false;

  do
  {
    if(ch >= BTS7960_MAX_CH) break;
    if(bts7960_tbl[ch].is_open != true) break;
    if(bts7960_tbl[ch].is_started == true)
    {
      ret = true;
      break;
    }

    if(pwmStart(bts7960_cfg[ch].rpwm_ch) != true) break;
    if(pwmStart(bts7960_cfg[ch].lpwm_ch) != true)
    {
      pwmStop(bts7960_cfg[ch].rpwm_ch);
      break;
    }

    bts7960_tbl[ch].is_started = true;
    ret = true;
  } while(0);

  bts7960Unlock();

  return ret;
}

bool bts7960Stop(uint8_t ch)
{
  bool ret = false;

  if(bts7960Lock() != true) return false;

  do
  {
    if(ch >= BTS7960_MAX_CH) break;
    if(bts7960_tbl[ch].is_open != true) break;

    ret = true;

    if(bts7960ApplyDuty(ch, BTS7960_DIR_STOP, 0U) != true)
    {
      ret = false;
    }

    if(bts7960_tbl[ch].is_started == true)
    {
      if(pwmStop(bts7960_cfg[ch].rpwm_ch) != true) ret = false;
      if(pwmStop(bts7960_cfg[ch].lpwm_ch) != true) ret = false;
    }

    bts7960_tbl[ch].is_started = false;
  } while(0);

  bts7960Unlock();

  return ret;
}

bool bts7960Run(uint8_t ch, int16_t speed)
{
  bool ret = false;

  if(bts7960Lock() != true) return false;

  do
  {
    if(ch >= BTS7960_MAX_CH) break;
    if(bts7960SetSpeed(ch, speed) != true) break;

    if(bts7960_tbl[ch].is_started != true)
    {
      if(bts7960Start(ch) != true) break;
    }

    ret = true;
  } while(0);

  bts7960Unlock();

  return ret;
}

bool bts7960Coast(uint8_t ch)
{
  return bts7960SetDuty(ch, BTS7960_DIR_STOP, 0U);
}

bool bts7960SetFreq(uint8_t ch, uint32_t freq_hz)
{
  bool ret = false;
  uint32_t prescaler;
  uint32_t period;

  if(bts7960Lock() != true) return false;

  do
  {
    if(ch >= BTS7960_MAX_CH) break;
    if(bts7960_tbl[ch].is_open != true) break;

    if(bts7960CalcTimer(freq_hz, &prescaler, &period) != true) break;

    if(pwmSetPrescaler(bts7960_cfg[ch].rpwm_ch, prescaler) != true) break;
    if(pwmSetPeriod(bts7960_cfg[ch].rpwm_ch, period) != true) break;
    if(pwmSetPrescaler(bts7960_cfg[ch].lpwm_ch, prescaler) != true) break;
    if(pwmSetPeriod(bts7960_cfg[ch].lpwm_ch, period) != true) break;

    bts7960_cfg[ch].freq_hz   = freq_hz;
    bts7960_tbl[ch].prescaler = prescaler;
    bts7960_tbl[ch].period    = period;

    ret = bts7960ApplyDuty(ch, bts7960_tbl[ch].dir, bts7960_tbl[ch].duty);
  } while(0);

  bts7960Unlock();

  return ret;
}

bool bts7960SetSpeed(uint8_t ch, int16_t speed)
{
  bool ret = false;
  bts7960_dir_t dir;
  uint8_t duty;

  if(bts7960Lock() != true) return false;

  do
  {
    if(ch >= BTS7960_MAX_CH) break;
    if(speed < -100) break;
    if(speed > 100) break;

    if(speed > 0)
    {
      dir = BTS7960_DIR_CW;
      duty = (uint8_t)speed;
    }
    else if(speed < 0)
    {
      dir = BTS7960_DIR_CCW;
      duty = (uint8_t)(-speed);
    }
    else
    {
      dir = BTS7960_DIR_STOP;
      duty = 0U;
    }

    ret = bts7960SetDuty(ch, dir, duty);
  } while(0);

  bts7960Unlock();

  return ret;
}

bool bts7960SetDuty(uint8_t ch, bts7960_dir_t dir, uint8_t duty)
{
  bool ret = false;

  if(bts7960Lock() != true) return false;

  do
  {
    if(ch >= BTS7960_MAX_CH) break;
    if(bts7960_tbl[ch].is_open != true) break;
    if(duty > 100U) break;
    if(dir > BTS7960_DIR_BRAKE) break;

    ret = bts7960ApplyDuty(ch, dir, duty);
  } while(0);

  bts7960Unlock();

  return ret;
}

bool bts7960Brake(uint8_t ch)
{
  return bts7960SetDuty(ch, BTS7960_DIR_BRAKE, 100U);
}

int16_t bts7960GetSpeed(uint8_t ch)
{
  int16_t ret = 0;

  if(bts7960Lock() != true) return 0;

  if(ch < BTS7960_MAX_CH)
  {
    ret = bts7960_tbl[ch].speed;
  }

  bts7960Unlock();

  return ret;
}

uint8_t bts7960GetDuty(uint8_t ch)
{
  uint8_t ret = 0U;

  if(bts7960Lock() != true) return 0U;

  if(ch < BTS7960_MAX_CH)
  {
    ret = bts7960_tbl[ch].duty;
  }

  bts7960Unlock();

  return ret;
}

bts7960_dir_t bts7960GetDir(uint8_t ch)
{
  bts7960_dir_t ret = BTS7960_DIR_STOP;

  if(bts7960Lock() != true) return BTS7960_DIR_STOP;

  if(ch < BTS7960_MAX_CH)
  {
    ret = bts7960_tbl[ch].dir;
  }

  bts7960Unlock();

  return ret;
}

bool bts7960ReadData(uint8_t ch, bts7960_data_t *p_data)
{
  bool ret = false;

  if(bts7960Lock() != true) return false;

  do
  {
    if(ch >= BTS7960_MAX_CH) break;
    if(p_data == NULL) break;

    p_data->is_open    = bts7960_tbl[ch].is_open;
    p_data->is_started = bts7960_tbl[ch].is_started;
    p_data->dir        = bts7960_tbl[ch].dir;
    p_data->speed      = bts7960_tbl[ch].speed;
    p_data->duty       = bts7960_tbl[ch].duty;
    p_data->freq_hz    = bts7960_cfg[ch].freq_hz;

    ret = true;
  } while(0);

  bts7960Unlock();

  return ret;
}

static bool bts7960Lock(void)
{
#ifdef _USE_HW_RTOS
  if(__get_IPSR() != 0U) return false;

  if((bts7960_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    return osMutexAcquire(bts7960_mutex, BTS7960_LOCK_TIMEOUT_MS) == osOK;
  }
#endif

  return true;
}

static void bts7960Unlock(void)
{
#ifdef _USE_HW_RTOS
  if((bts7960_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    (void)osMutexRelease(bts7960_mutex);
  }
#endif
}

static bool bts7960IsValidConfig(const bts7960_cfg_t *p_cfg)
{
  if(p_cfg == NULL) return false;
  if(p_cfg->rpwm_ch >= PWM_MAX_CH) return false;
  if(p_cfg->lpwm_ch >= PWM_MAX_CH) return false;
  if(p_cfg->rpwm_ch == p_cfg->lpwm_ch) return false;
  if(p_cfg->freq_hz == 0U) return false;

  return true;
}

static bool bts7960ApplyDuty(uint8_t ch, bts7960_dir_t dir, uint8_t duty)
{
  uint32_t active_pulse;

  active_pulse = bts7960DutyToPulse(bts7960_tbl[ch].period, duty);

  switch(dir)
  {
    case BTS7960_DIR_STOP:
      if(pwmSetPulse(bts7960_cfg[ch].rpwm_ch, 0U) != true) return false;
      if(pwmSetPulse(bts7960_cfg[ch].lpwm_ch, 0U) != true) return false;
      bts7960_tbl[ch].speed = 0;
      bts7960_tbl[ch].duty  = 0U;
      break;

    case BTS7960_DIR_CW:
      if(pwmSetPulse(bts7960_cfg[ch].lpwm_ch, 0U) != true) return false;
      if(pwmSetPulse(bts7960_cfg[ch].rpwm_ch, active_pulse) != true) return false;
      bts7960_tbl[ch].speed = (int16_t)duty;
      bts7960_tbl[ch].duty  = duty;
      break;

    case BTS7960_DIR_CCW:
      if(pwmSetPulse(bts7960_cfg[ch].rpwm_ch, 0U) != true) return false;
      if(pwmSetPulse(bts7960_cfg[ch].lpwm_ch, active_pulse) != true) return false;
      bts7960_tbl[ch].speed = -(int16_t)duty;
      bts7960_tbl[ch].duty  = duty;
      break;

    case BTS7960_DIR_BRAKE:
      if(pwmSetPulse(bts7960_cfg[ch].rpwm_ch, active_pulse) != true) return false;
      if(pwmSetPulse(bts7960_cfg[ch].lpwm_ch, active_pulse) != true) return false;
      bts7960_tbl[ch].speed = 0;
      bts7960_tbl[ch].duty  = duty;
      break;

    default:
      return false;
  }

  bts7960_tbl[ch].dir = dir;

  return true;
}

static bool bts7960CalcTimer(uint32_t freq_hz, uint32_t *p_prescaler, uint32_t *p_period)
{
  uint32_t timer_clk;
  uint64_t prescaler;
  uint64_t period;
  uint64_t max_period;

  if(freq_hz == 0U) return false;
  if(p_prescaler == NULL) return false;
  if(p_period == NULL) return false;

  timer_clk = HAL_RCC_GetHCLKFreq();
  if(timer_clk == 0U) return false;

  max_period = 0x10000ULL;
  prescaler = ((uint64_t)timer_clk + ((uint64_t)freq_hz * max_period) - 1ULL)
              / ((uint64_t)freq_hz * max_period);

  if(prescaler == 0ULL)
  {
    prescaler = 1ULL;
  }

  if(prescaler > 0x10000ULL) return false;

  period = (uint64_t)timer_clk / (prescaler * (uint64_t)freq_hz);
  if(period < 2ULL) return false;
  if(period > max_period) return false;

  *p_prescaler = (uint32_t)(prescaler - 1ULL);
  *p_period    = (uint32_t)(period - 1ULL);

  return true;
}

static uint32_t bts7960DutyToPulse(uint32_t period, uint8_t duty)
{
  uint32_t pulse;

  if(duty == 0U) return 0U;
  if(duty >= 100U) return period;

  pulse = (((period + 1U) * duty) + 50U) / 100U;
  if(pulse > 0U)
  {
    pulse--;
  }

  if(pulse > period)
  {
    pulse = period;
  }

  return pulse;
}

#ifdef _USE_HW_CLI
static const char *bts7960DirToStr(bts7960_dir_t dir)
{
  switch(dir)
  {
    case BTS7960_DIR_STOP:
      return "stop";

    case BTS7960_DIR_CW:
      return "cw";

    case BTS7960_DIR_CCW:
      return "ccw";

    case BTS7960_DIR_BRAKE:
      return "brake";

    default:
      return "unknown";
  }
}

static void cliBts7960(cli_args_t *args)
{
  bool ret = false;
  bool cmd_ret;
  uint8_t ch;
  int16_t speed;
  uint32_t value;

  if(args->argc == 1)
  {
    if(args->isStr(0, "show") == true)
    {
      for(uint8_t i = 0; i < BTS7960_MAX_CH; i++)
      {
        bts7960_data_t data;

        if(bts7960ReadData(i, &data) == true)
        {
          cliPrintf("bts7960 %d open:%d start:%d dir:%s speed:%d duty:%u freq:%luhz\n",
                    i,
                    data.is_open,
                    data.is_started,
                    bts7960DirToStr(data.dir),
                    data.speed,
                    (unsigned int)data.duty,
                    data.freq_hz);
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
      cmd_ret = bts7960Open(ch);
      cliPrintf("bts7960 open %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "start") == true)
    {
      cmd_ret = bts7960Start(ch);
      cliPrintf("bts7960 start %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "stop") == true)
    {
      cmd_ret = bts7960Stop(ch);
      cliPrintf("bts7960 stop %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "coast") == true)
    {
      cmd_ret = bts7960Coast(ch);
      cliPrintf("bts7960 coast %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "brake") == true)
    {
      cmd_ret = bts7960Brake(ch);
      cliPrintf("bts7960 brake %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }
  }

  if(args->argc == 3)
  {
    ch    = (uint8_t)args->getData(1);
    value = (uint32_t)args->getData(2);

    if(args->isStr(0, "speed") == true)
    {
      speed = (int16_t)args->getData(2);
      cmd_ret = bts7960SetSpeed(ch, speed);
      cliPrintf("bts7960 speed %d %d : %s\n", ch, speed, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "run") == true)
    {
      speed = (int16_t)args->getData(2);
      cmd_ret = bts7960Run(ch, speed);
      cliPrintf("bts7960 run %d %d : %s\n", ch, speed, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "freq") == true)
    {
      cmd_ret = bts7960SetFreq(ch, value);
      cliPrintf("bts7960 freq %d %luhz : %s\n", ch, value, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }
  }

  if(args->argc == 4)
  {
    ch    = (uint8_t)args->getData(1);
    value = (uint32_t)args->getData(2);

    if(args->isStr(0, "duty") == true)
    {
      uint8_t duty = (uint8_t)args->getData(3);
      bts7960_dir_t dir = BTS7960_DIR_STOP;

      if(args->isStr(2, "cw") == true)
      {
        dir = BTS7960_DIR_CW;
      }
      else if(args->isStr(2, "ccw") == true)
      {
        dir = BTS7960_DIR_CCW;
      }
      else if(args->isStr(2, "brake") == true)
      {
        dir = BTS7960_DIR_BRAKE;
      }

      cmd_ret = bts7960SetDuty(ch, dir, duty);
      cliPrintf("bts7960 duty %d %s %u : %s\n",
                ch,
                bts7960DirToStr(dir),
                (unsigned int)duty,
                cmd_ret ? "OK" : "FAIL");
      ret = true;
    }
  }

  if(ret != true)
  {
    cliPrintf("bts7960 show\n");
    cliPrintf("bts7960 open ch[0~%d]\n", BTS7960_MAX_CH - 1);
    cliPrintf("bts7960 start ch[0~%d]\n", BTS7960_MAX_CH - 1);
    cliPrintf("bts7960 stop ch[0~%d]\n", BTS7960_MAX_CH - 1);
    cliPrintf("bts7960 coast ch[0~%d]\n", BTS7960_MAX_CH - 1);
    cliPrintf("bts7960 brake ch[0~%d]\n", BTS7960_MAX_CH - 1);
    cliPrintf("bts7960 run ch[0~%d] speed[-100~100]\n", BTS7960_MAX_CH - 1);
    cliPrintf("bts7960 speed ch[0~%d] speed[-100~100]\n", BTS7960_MAX_CH - 1);
    cliPrintf("bts7960 freq ch[0~%d] hz\n", BTS7960_MAX_CH - 1);
    cliPrintf("bts7960 duty ch[0~%d] cw:ccw:brake duty[0~100]\n", BTS7960_MAX_CH - 1);
  }
}
#endif

#endif
