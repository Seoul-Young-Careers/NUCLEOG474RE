/*
 * button.h
 *
 *  Created on: Aug 7, 2025
 *      Author: young
 */

#ifndef SRC_COMMON_HW_INCLUDE_BUTTON_H_
#define SRC_COMMON_HW_INCLUDE_BUTTON_H_

#include "hw_def.h"

#ifdef _USE_HW_BUTTON

#define BUTTON_MAX_CH			HW_BUTTON_MAX_CH
#define BUTTON_LOCK_TIMEOUT_MS  100U

typedef struct
{
  bool is_pressed;
} button_data_t;

bool buttonInit(void);
bool buttonGetPressed(uint8_t ch);
bool buttonReadData(uint8_t ch, button_data_t *p_data);

#endif

#endif /* SRC_COMMON_HW_INCLUDE_BUTTON_H_ */
