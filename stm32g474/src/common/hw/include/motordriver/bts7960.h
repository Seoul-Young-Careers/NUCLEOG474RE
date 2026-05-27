/*
 * bts7960.h
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#ifndef SRC_COMMON_HW_INCLUDE_MOTORDRIVER_BTS7960_H_
#define SRC_COMMON_HW_INCLUDE_MOTORDRIVER_BTS7960_H_

#include "hw_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _USE_BTS7960

#define BTS7960_MAX_CH                 HW_BTS7960_MAX_CH
#define BTS7960_FREQ_HZ                20000U
#define BTS7960_LOCK_TIMEOUT_MS        100U


typedef enum
{
  BTS7960_DIR_STOP = 0,
  BTS7960_DIR_CW,
  BTS7960_DIR_CCW,
  BTS7960_DIR_BRAKE,
} bts7960_dir_t;

typedef struct
{
  uint8_t rpwm_ch;
  uint8_t lpwm_ch;
  uint32_t freq_hz;
} bts7960_cfg_t;

typedef struct
{
  bool is_open;
  bool is_started;

  bts7960_dir_t dir;
  int16_t speed;
  uint8_t duty;
  uint32_t freq_hz;
} bts7960_data_t;

bool bts7960Init(void);
bool bts7960Open(uint8_t ch);

bool bts7960SetConfig(uint8_t ch, const bts7960_cfg_t *p_cfg);
bool bts7960ReadConfig(uint8_t ch, bts7960_cfg_t *p_cfg);

bool bts7960IsOpen(uint8_t ch);
bool bts7960IsStarted(uint8_t ch);

bool bts7960Start(uint8_t ch);
bool bts7960Stop(uint8_t ch);
bool bts7960Run(uint8_t ch, int16_t speed);
bool bts7960Coast(uint8_t ch);

bool bts7960SetFreq(uint8_t ch, uint32_t freq_hz);
bool bts7960SetSpeed(uint8_t ch, int16_t speed);
bool bts7960SetDuty(uint8_t ch, bts7960_dir_t dir, uint8_t duty);
bool bts7960Brake(uint8_t ch);

int16_t bts7960GetSpeed(uint8_t ch);
uint8_t bts7960GetDuty(uint8_t ch);
bts7960_dir_t bts7960GetDir(uint8_t ch);
bool bts7960ReadData(uint8_t ch, bts7960_data_t *p_data);

#endif

#ifdef __cplusplus
}
#endif


#endif /* SRC_COMMON_HW_INCLUDE_MOTORDRIVER_BTS7960_H_ */
