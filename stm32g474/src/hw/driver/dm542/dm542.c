/*
 * dm542.c
 *
 *  Created on: May 16, 2026
 *      Author: young
 */


#include "DM542/dm542.h"
#include "pwm.h"
#include "cli.h"

#ifdef _USE_DM542

#define _PIN_GPIO_DM542_PUL             _DEF_PWM2
#define _PIN_GPIO_DM542_DIR             0
#define _PIN_GPIO_DM542_ENA             1

typedef struct
{
  uint8_t pul_ch;
  uint8_t dir_ch;
  uint8_t ena_ch;

  bool is_open;
  bool is_busy;
  bool is_enabled;

  dm542_dir_t dir;
  int32_t position_step;
  uint32_t remain_step;
} dm542_tbl_t;

static dm542_tbl_t dm542_tbl[DM542_MAX_CH] =
{
  {
    .pul_ch         = _PIN_GPIO_DM542_PUL,
    .dir_ch         = _PIN_GPIO_DM542_DIR,
    .ena_ch         = _PIN_GPIO_DM542_ENA,

    .is_open        = false,
    .is_busy        = false,
    .is_enabled     = false,

    .dir            = DM542_DIR_CW,
    .position_step  = 0,
    .remain_step    = 0,
  },
};

#endif
