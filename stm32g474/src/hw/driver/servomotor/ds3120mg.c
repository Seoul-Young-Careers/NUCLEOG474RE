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

static uint16_t ds3120mgAngleToPulse(uint8_t ch, float angle_deg);
static float ds3120mgPulseToAngle(uint8_t ch, uint16_t pulse_us);

#ifdef _USE_HW_CLI
static int32_t ds3120mgFloatToTenth(float value);
#endif

bool ds3120mgInit(void)
{
  bool ret = true;

  for(uint8_t i = 0; i < DS3120MG_MAX_CH; i++)
  {
    ds3120mg_tbl[i].is_open       = false;
    ds3120mg_tbl[i].is_started    = false;
    ds3120mg_tbl[i].freq_hz       = DS3120MG_DEFAULT_FREQ_HZ;
    ds3120mg_tbl[i].min_pulse_us  = DS3120MG_MIN_PULSE_US;
    ds3120mg_tbl[i].mid_pulse_us  = DS3120MG_MID_PULSE_US;
    ds3120mg_tbl[i].max_pulse_us  = DS3120MG_MAX_PULSE_US;
    ds3120mg_tbl[i].pulse_us      = DS3120MG_MID_PULSE_US;
    ds3120mg_tbl[i].angle_deg     = DS3120MG_DEFAULT_ANGLE_DEG;
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
  if(ch >= DS3120MG_MAX_CH) return false;
  if(ds3120mg_tbl[ch].is_open == true) return true;

  switch(ch)
  {
    case _DEF_DS3120MG1:
      if(pwmOpen(_DEF_PWM3) != true)
      {
        return false;
      }

      if(pwmSetPulse(_DEF_PWM3, ds3120mg_tbl[ch].pulse_us) != true)
      {
        return false;
      }
      break;

    case _DEF_DS3120MG2:
      if(pwmOpen(_DEF_PWM1) != true)
      {
        return false;
      }

      if(pwmSetPulse(_DEF_PWM1, ds3120mg_tbl[ch].pulse_us) != true)
      {
        return false;
      }
      break;

    default:
      return false;
  }

  ds3120mg_tbl[ch].is_open = true;

  return true;
}

bool ds3120mgIsOpen(uint8_t ch)
{
  if(ch >= DS3120MG_MAX_CH) return false;

  return ds3120mg_tbl[ch].is_open;
}

bool ds3120mgIsStarted(uint8_t ch)
{
  if(ch >= DS3120MG_MAX_CH) return false;

  return ds3120mg_tbl[ch].is_started;
}

bool ds3120mgStart(uint8_t ch)
{
  if(ch >= DS3120MG_MAX_CH) return false;
  if(ds3120mg_tbl[ch].is_open != true) return false;

  switch(ch)
  {
    case _DEF_DS3120MG1:
      if(pwmStart(_DEF_PWM3) != true)
      {
        return false;
      }
      break;

    case _DEF_DS3120MG2:
      if(pwmStart(_DEF_PWM1) != true)
      {
        return false;
      }
      break;

    default:
      return false;
  }

  ds3120mg_tbl[ch].is_started = true;

  return true;
}

bool ds3120mgStop(uint8_t ch)
{
  if(ch >= DS3120MG_MAX_CH) return false;
  if(ds3120mg_tbl[ch].is_open != true) return false;

  switch(ch)
  {
    case _DEF_DS3120MG1:
      if(pwmStop(_DEF_PWM3) != true)
      {
        return false;
      }
      break;

    case _DEF_DS3120MG2:
      if(pwmStop(_DEF_PWM1) != true)
      {
        return false;
      }
      break;

    default:
      return false;
  }

  ds3120mg_tbl[ch].is_started = false;

  return true;
}

bool ds3120mgSetPulseUs(uint8_t ch, uint16_t pulse_us)
{
  if(ch >= DS3120MG_MAX_CH) return false;
  if(ds3120mg_tbl[ch].is_open != true) return false;
  if(pulse_us < ds3120mg_tbl[ch].min_pulse_us) return false;
  if(pulse_us > ds3120mg_tbl[ch].max_pulse_us) return false;

  switch(ch)
  {
    case _DEF_DS3120MG1:
      if(pwmSetPulse(_DEF_PWM3, pulse_us) != true)
      {
        return false;
      }
      break;

    case _DEF_DS3120MG2:
      if(pwmSetPulse(_DEF_PWM1, pulse_us) != true)
      {
        return false;
      }
      break;

    default:
      return false;
  }

  ds3120mg_tbl[ch].pulse_us  = pulse_us;
  ds3120mg_tbl[ch].angle_deg = ds3120mgPulseToAngle(ch, pulse_us);

  return true;
}

bool ds3120mgSetAngle(uint8_t ch, float angle_deg)
{
  if(ch >= DS3120MG_MAX_CH) return false;
  if(angle_deg < 0.0f) return false;
  if(angle_deg > ds3120mg_tbl[ch].max_angle_deg) return false;

  return ds3120mgSetPulseUs(ch, ds3120mgAngleToPulse(ch, angle_deg));
}

bool ds3120mgCenter(uint8_t ch)
{
  if(ch >= DS3120MG_MAX_CH) return false;

  return ds3120mgSetPulseUs(ch, ds3120mg_tbl[ch].mid_pulse_us);
}

uint16_t ds3120mgGetPulseUs(uint8_t ch)
{
  if(ch >= DS3120MG_MAX_CH) return 0U;

  return ds3120mg_tbl[ch].pulse_us;
}

float ds3120mgGetAngle(uint8_t ch)
{
  if(ch >= DS3120MG_MAX_CH) return 0.0f;

  return ds3120mg_tbl[ch].angle_deg;
}

bool ds3120mgReadData(uint8_t ch, ds3120mg_data_t *p_data)
{
  if(ch >= DS3120MG_MAX_CH) return false;
  if(p_data == NULL) return false;

  p_data->freq_hz       = ds3120mg_tbl[ch].freq_hz;
  p_data->min_pulse_us  = ds3120mg_tbl[ch].min_pulse_us;
  p_data->mid_pulse_us  = ds3120mg_tbl[ch].mid_pulse_us;
  p_data->max_pulse_us  = ds3120mg_tbl[ch].max_pulse_us;
  p_data->pulse_us      = ds3120mg_tbl[ch].pulse_us;
  p_data->angle_deg     = ds3120mg_tbl[ch].angle_deg;
  p_data->max_angle_deg = ds3120mg_tbl[ch].max_angle_deg;

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
        int32_t angle_x10 = ds3120mgFloatToTenth(ds3120mg_tbl[i].angle_deg);
        int32_t max_angle_x10 = ds3120mgFloatToTenth(ds3120mg_tbl[i].max_angle_deg);

        cliPrintf("servo %d open:%d start:%d freq:%uhz pulse:%uus angle:%ld.%ld/%ld.%ld range:%u-%u-%u\n",
                  i,
                  ds3120mg_tbl[i].is_open,
                  ds3120mg_tbl[i].is_started,
                  (unsigned int)ds3120mg_tbl[i].freq_hz,
                  (unsigned int)ds3120mg_tbl[i].pulse_us,
                  (long)(angle_x10 / 10),
                  (long)(angle_x10 % 10),
                  (long)(max_angle_x10 / 10),
                  (long)(max_angle_x10 % 10),
                  (unsigned int)ds3120mg_tbl[i].min_pulse_us,
                  (unsigned int)ds3120mg_tbl[i].mid_pulse_us,
                  (unsigned int)ds3120mg_tbl[i].max_pulse_us);
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
    cliPrintf("ds3120mg pulse ch[0~%d] us[%u~%u]\n",
              DS3120MG_MAX_CH - 1,
              (unsigned int)DS3120MG_MIN_PULSE_US,
              (unsigned int)DS3120MG_MAX_PULSE_US);
    cliPrintf("ds3120mg angle ch[0~%d] deg\n", DS3120MG_MAX_CH - 1);
  }
}
#endif

#endif
