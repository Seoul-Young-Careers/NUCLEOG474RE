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
  }

  if(dm542Open(DM542_PUL) != true)
  {
    ret = false;
  }
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

#endif
