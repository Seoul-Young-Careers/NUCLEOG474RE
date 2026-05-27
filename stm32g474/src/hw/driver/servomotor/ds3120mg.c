/*
 * ds3120mg.c
 *
 *  Created on: May 20, 2026
 *      Author: young
 */

#include <servomotor/ds3120mg.h>
#include "pwm.h"

#ifdef _USE_DS3120MG

#ifdef _USE_HW_CLI
#include "cli.h"

static void cliDs3120mg(cli_args_t *args);
#endif

typedef struct
{
  bool is_open;
  bool is_started;

  uint16_t freq_hz;
  uint16_t min_pulse_us;
  uint16_t mid_pulse_us;
  uint16_t max_pulse_us;
  uint16_t pulse_us;
  float angle_deg;
  float max_angle_deg;
} ds3120mg_tbl_t;

static ds3120mg_tbl_t ds3120mg_tbl[DS3120MG_MAX_CH];

#ifdef _USE_HW_RTOS
static osMutexId_t ds3120mg_mutex = NULL;
static const osMutexAttr_t ds3120mg_mutex_attr =
{
  .name      = "ds3120mg",
  .attr_bits = osMutexRecursive | osMutexPrioInherit,
};
#endif

static bool ds3120mgLock(void);
static void ds3120mgUnlock(void);
static bool ds3120mgGetPwmCh(uint8_t ch, uint8_t *p_pwm_ch);
static uint16_t ds3120mgAngleToPulse(uint8_t ch, float angle_deg);
static float ds3120mgPulseToAngle(uint8_t ch, uint16_t pulse_us);

#ifdef _USE_HW_CLI
static int32_t ds3120mgFloatToTenth(float value);
#endif

bool ds3120mgInit(void)
{
  bool ret = true;

#ifdef _USE_HW_RTOS
  if(ds3120mg_mutex == NULL)
  {
    ds3120mg_mutex = osMutexNew(&ds3120mg_mutex_attr);
    if(ds3120mg_mutex == NULL)
    {
      ret = false;
    }
  }
#endif

  for(uint8_t i = 0; i < DS3120MG_MAX_CH; i++)
  {
    ds3120mg_tbl[i].is_open       = false;
    ds3120mg_tbl[i].is_started    = false;
    ds3120mg_tbl[i].freq_hz       = DS3120MG_FREQ_HZ;
    ds3120mg_tbl[i].min_pulse_us  = DS3120MG_MIN_PULSE_US;
    ds3120mg_tbl[i].mid_pulse_us  = DS3120MG_MID_PULSE_US;
    ds3120mg_tbl[i].max_pulse_us  = DS3120MG_MAX_PULSE_US;
    ds3120mg_tbl[i].pulse_us      = DS3120MG_MID_PULSE_US;
    ds3120mg_tbl[i].angle_deg     = DS3120MG_ANGLE_DEG;
    ds3120mg_tbl[i].max_angle_deg = DS3120MG_MAX_ANGLE_DEG;

    if(ds3120mgOpen(i) != true)
    {
      ret = false;
    }
  }

#ifdef _USE_HW_CLI
  cliAdd("ds3120mg", cliDs3120mg);
#endif

  return ret;
}

bool ds3120mgOpen(uint8_t ch)
{
  bool ret = false;
  uint8_t pwm_ch;

  if(ds3120mgLock() != true) return false;

  do
  {
    if(ch >= DS3120MG_MAX_CH) break;
    if(ds3120mg_tbl[ch].is_open == true)
    {
      ret = true;
      break;
    }
    if(ds3120mgGetPwmCh(ch, &pwm_ch) != true) break;

    if(pwmOpen(pwm_ch) != true) break;
    if(pwmSetPulse(pwm_ch, ds3120mg_tbl[ch].pulse_us) != true) break;

    ds3120mg_tbl[ch].is_open = true;
    ret = true;
  } while(0);

  ds3120mgUnlock();

  return ret;
}

bool ds3120mgIsOpen(uint8_t ch)
{
  bool ret = false;

  if(ds3120mgLock() != true) return false;

  if(ch < DS3120MG_MAX_CH)
  {
    ret = ds3120mg_tbl[ch].is_open;
  }

  ds3120mgUnlock();

  return ret;
}

bool ds3120mgIsStarted(uint8_t ch)
{
  bool ret = false;

  if(ds3120mgLock() != true) return false;

  if(ch < DS3120MG_MAX_CH)
  {
    ret = ds3120mg_tbl[ch].is_started;
  }

  ds3120mgUnlock();

  return ret;
}

bool ds3120mgStart(uint8_t ch)
{
  bool ret = false;
  uint8_t pwm_ch;

  if(ds3120mgLock() != true) return false;

  do
  {
    if(ch >= DS3120MG_MAX_CH) break;
    if(ds3120mg_tbl[ch].is_open != true) break;
    if(ds3120mg_tbl[ch].is_started == true)
    {
      ret = true;
      break;
    }
    if(ds3120mgGetPwmCh(ch, &pwm_ch) != true) break;
    if(pwmStart(pwm_ch) != true) break;

    ds3120mg_tbl[ch].is_started = true;
    ret = true;
  } while(0);

  ds3120mgUnlock();

  return ret;
}

bool ds3120mgStop(uint8_t ch)
{
  bool ret = false;
  uint8_t pwm_ch;

  if(ds3120mgLock() != true) return false;

  do
  {
    if(ch >= DS3120MG_MAX_CH) break;
    if(ds3120mg_tbl[ch].is_open != true) break;
    if(ds3120mg_tbl[ch].is_started != true)
    {
      ret = true;
      break;
    }
    if(ds3120mgGetPwmCh(ch, &pwm_ch) != true) break;
    if(pwmStop(pwm_ch) != true) break;

    ds3120mg_tbl[ch].is_started = false;
    ret = true;
  } while(0);

  ds3120mgUnlock();

  return ret;
}

bool ds3120mgSetPulseUs(uint8_t ch, uint16_t pulse_us)
{
  bool ret = false;
  uint8_t pwm_ch;

  if(ds3120mgLock() != true) return false;

  do
  {
    if(ch >= DS3120MG_MAX_CH) break;
    if(ds3120mg_tbl[ch].is_open != true) break;
    if(pulse_us < ds3120mg_tbl[ch].min_pulse_us) break;
    if(pulse_us > ds3120mg_tbl[ch].max_pulse_us) break;
    if(ds3120mgGetPwmCh(ch, &pwm_ch) != true) break;
    if(pwmSetPulse(pwm_ch, pulse_us) != true) break;

    ds3120mg_tbl[ch].pulse_us  = pulse_us;
    ds3120mg_tbl[ch].angle_deg = ds3120mgPulseToAngle(ch, pulse_us);
    ret = true;
  } while(0);

  ds3120mgUnlock();

  return ret;
}

bool ds3120mgSetAngle(uint8_t ch, float angle_deg)
{
  bool ret = false;

  if(ds3120mgLock() != true) return false;

  do
  {
    if(ch >= DS3120MG_MAX_CH) break;
    if(angle_deg < 0.0f) break;
    if(angle_deg > ds3120mg_tbl[ch].max_angle_deg) break;

    ret = ds3120mgSetPulseUs(ch, ds3120mgAngleToPulse(ch, angle_deg));
  } while(0);

  ds3120mgUnlock();

  return ret;
}

bool ds3120mgCenter(uint8_t ch)
{
  bool ret = false;

  if(ds3120mgLock() != true) return false;

  do
  {
    if(ch >= DS3120MG_MAX_CH) break;

    ret = ds3120mgSetPulseUs(ch, ds3120mg_tbl[ch].mid_pulse_us);
  } while(0);

  ds3120mgUnlock();

  return ret;
}

bool ds3120mgRun(uint8_t ch, float angle_deg)
{
  bool ret = false;

  if(ds3120mgLock() != true) return false;

  do
  {
    if(ds3120mgSetAngle(ch, angle_deg) != true) break;
    if(ds3120mgStart(ch) != true) break;

    ret = true;
  } while(0);

  ds3120mgUnlock();

  return ret;
}

uint16_t ds3120mgGetPulseUs(uint8_t ch)
{
  uint16_t ret = 0U;

  if(ds3120mgLock() != true) return 0U;

  if(ch < DS3120MG_MAX_CH)
  {
    ret = ds3120mg_tbl[ch].pulse_us;
  }

  ds3120mgUnlock();

  return ret;
}

float ds3120mgGetAngle(uint8_t ch)
{
  float ret = 0.0f;

  if(ds3120mgLock() != true) return 0.0f;

  if(ch < DS3120MG_MAX_CH)
  {
    ret = ds3120mg_tbl[ch].angle_deg;
  }

  ds3120mgUnlock();

  return ret;
}

bool ds3120mgReadData(uint8_t ch, ds3120mg_data_t *p_data)
{
  bool ret = false;

  if(ds3120mgLock() != true) return false;

  do
  {
    if(ch >= DS3120MG_MAX_CH) break;
    if(p_data == NULL) break;

    p_data->is_open       = ds3120mg_tbl[ch].is_open;
    p_data->is_started    = ds3120mg_tbl[ch].is_started;
    p_data->freq_hz       = ds3120mg_tbl[ch].freq_hz;
    p_data->min_pulse_us  = ds3120mg_tbl[ch].min_pulse_us;
    p_data->mid_pulse_us  = ds3120mg_tbl[ch].mid_pulse_us;
    p_data->max_pulse_us  = ds3120mg_tbl[ch].max_pulse_us;
    p_data->pulse_us      = ds3120mg_tbl[ch].pulse_us;
    p_data->angle_deg     = ds3120mg_tbl[ch].angle_deg;
    p_data->max_angle_deg = ds3120mg_tbl[ch].max_angle_deg;

    ret = true;
  } while(0);

  ds3120mgUnlock();

  return ret;
}

static bool ds3120mgLock(void)
{
#ifdef _USE_HW_RTOS
  if(__get_IPSR() != 0U) return false;

  if((ds3120mg_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    return osMutexAcquire(ds3120mg_mutex, DS3120MG_LOCK_TIMEOUT_MS) == osOK;
  }
#endif

  return true;
}

static void ds3120mgUnlock(void)
{
#ifdef _USE_HW_RTOS
  if((ds3120mg_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    (void)osMutexRelease(ds3120mg_mutex);
  }
#endif
}

static bool ds3120mgGetPwmCh(uint8_t ch, uint8_t *p_pwm_ch)
{
  if(p_pwm_ch == NULL) return false;

  switch(ch)
  {
    case _DEF_DS3120MG1:
      *p_pwm_ch = _DEF_PWM3;
      break;

    case _DEF_DS3120MG2:
      *p_pwm_ch = _DEF_PWM1;
      break;

    default:
      return false;
  }

  return true;
}

static uint16_t ds3120mgAngleToPulse(uint8_t ch, float angle_deg)
{
  float pulse_f;

  if(angle_deg < 0.0f)
  {
    angle_deg = 0.0f;
  }

  if(angle_deg > ds3120mg_tbl[ch].max_angle_deg)
  {
    angle_deg = ds3120mg_tbl[ch].max_angle_deg;
  }

  pulse_f = (float)ds3120mg_tbl[ch].min_pulse_us;
  pulse_f += ((float)(ds3120mg_tbl[ch].max_pulse_us - ds3120mg_tbl[ch].min_pulse_us) * angle_deg)
             / ds3120mg_tbl[ch].max_angle_deg;

  return (uint16_t)(pulse_f + 0.5f);
}

static float ds3120mgPulseToAngle(uint8_t ch, uint16_t pulse_us)
{
  if(pulse_us <= ds3120mg_tbl[ch].min_pulse_us)
  {
    return 0.0f;
  }

  if(pulse_us >= ds3120mg_tbl[ch].max_pulse_us)
  {
    return ds3120mg_tbl[ch].max_angle_deg;
  }

  return ((float)(pulse_us - ds3120mg_tbl[ch].min_pulse_us) * ds3120mg_tbl[ch].max_angle_deg)
         / (float)(ds3120mg_tbl[ch].max_pulse_us - ds3120mg_tbl[ch].min_pulse_us);
}

#ifdef _USE_HW_CLI
static int32_t ds3120mgFloatToTenth(float value)
{
  if(value >= 0.0f)
  {
    return (int32_t)((value * 10.0f) + 0.5f);
  }

  return (int32_t)((value * 10.0f) - 0.5f);
}

static void cliDs3120mg(cli_args_t *args)
{
  bool ret = false;
  bool cmd_ret;
  uint8_t ch;
  uint16_t value;

  if(args->argc == 1)
  {
    if(args->isStr(0, "show") == true)
    {
      for(uint8_t i = 0; i < DS3120MG_MAX_CH; i++)
      {
        ds3120mg_data_t data;

        if(ds3120mgReadData(i, &data) == true)
        {
          int32_t angle_x10 = ds3120mgFloatToTenth(data.angle_deg);
          int32_t max_angle_x10 = ds3120mgFloatToTenth(data.max_angle_deg);

          cliPrintf("servo %d open:%d start:%d freq:%uhz pulse:%uus angle:%ld.%ld/%ld.%ld range:%u-%u-%u\n",
                    i,
                    data.is_open,
                    data.is_started,
                    (unsigned int)data.freq_hz,
                    (unsigned int)data.pulse_us,
                    (long)(angle_x10 / 10),
                    (long)(angle_x10 % 10),
                    (long)(max_angle_x10 / 10),
                    (long)(max_angle_x10 % 10),
                    (unsigned int)data.min_pulse_us,
                    (unsigned int)data.mid_pulse_us,
                    (unsigned int)data.max_pulse_us);
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
      cmd_ret = ds3120mgOpen(ch);
      cliPrintf("ds3120mg open %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "start") == true)
    {
      cmd_ret = ds3120mgStart(ch);
      cliPrintf("ds3120mg start %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "stop") == true)
    {
      cmd_ret = ds3120mgStop(ch);
      cliPrintf("ds3120mg stop %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "center") == true)
    {
      cmd_ret = ds3120mgCenter(ch);
      cliPrintf("ds3120mg center %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }
  }

  if(args->argc == 3)
  {
    ch = (uint8_t)args->getData(1);

    if(args->isStr(0, "pulse") == true)
    {
      value = (uint16_t)args->getData(2);
      cmd_ret = ds3120mgSetPulseUs(ch, value);
      cliPrintf("ds3120mg pulse %d %uus : %s\n", ch, (unsigned int)value, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "run") == true)
    {
      float angle = args->getFloat(2);
      int32_t angle_x10 = ds3120mgFloatToTenth(angle);

      cmd_ret = ds3120mgRun(ch, angle);
      cliPrintf("ds3120mg run %d %ld.%ld : %s\n",
                ch,
                (long)(angle_x10 / 10),
                (long)(angle_x10 % 10),
                cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "angle") == true)
    {
      float angle = args->getFloat(2);
      int32_t angle_x10 = ds3120mgFloatToTenth(angle);

      cmd_ret = ds3120mgSetAngle(ch, angle);
      cliPrintf("ds3120mg angle %d %ld.%ld : %s\n",
                ch,
                (long)(angle_x10 / 10),
                (long)(angle_x10 % 10),
                cmd_ret ? "OK" : "FAIL");
      ret = true;
    }
  }

  if(ret != true)
  {
    cliPrintf("ds3120mg show\n");
    cliPrintf("ds3120mg open ch[0~%d]\n", DS3120MG_MAX_CH - 1);
    cliPrintf("ds3120mg start ch[0~%d]\n", DS3120MG_MAX_CH - 1);
    cliPrintf("ds3120mg stop ch[0~%d]\n", DS3120MG_MAX_CH - 1);
    cliPrintf("ds3120mg center ch[0~%d]\n", DS3120MG_MAX_CH - 1);
    cliPrintf("ds3120mg run ch[0~%d] deg\n", DS3120MG_MAX_CH - 1);
    cliPrintf("ds3120mg pulse ch[0~%d] us[%u~%u]\n",
              DS3120MG_MAX_CH - 1,
              (unsigned int)DS3120MG_MIN_PULSE_US,
              (unsigned int)DS3120MG_MAX_PULSE_US);
    cliPrintf("ds3120mg angle ch[0~%d] deg\n", DS3120MG_MAX_CH - 1);
  }
}
#endif

#endif
