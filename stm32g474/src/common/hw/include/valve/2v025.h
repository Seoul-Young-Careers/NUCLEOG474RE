/*
 * 2v025.h
 *
 *  Created on: May 22, 2026
 *      Author: young
 */

#ifndef SRC_COMMON_HW_INCLUDE_VALVE_2V025_H_
#define SRC_COMMON_HW_INCLUDE_VALVE_2V025_H_

#include "hw_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _USE_2V025

#define V025_MAX_CH                    HW_2V025_MAX_CH
#define V025_LOCK_TIMEOUT_MS           100U

typedef struct
{
  bool is_ready;
  bool is_open;
} v025_data_t;

bool v025Init(void);
bool v025IsReady(uint8_t ch);

bool v025ValveOpen(uint8_t ch);
bool v025ValveClose(uint8_t ch);
bool v025ValveSet(uint8_t ch, bool open);
bool v025ValveToggle(uint8_t ch);
bool v025ValveIsOpen(uint8_t ch);
bool v025ReadData(uint8_t ch, v025_data_t *p_data);

#endif

#ifdef __cplusplus
}
#endif

#endif /* SRC_COMMON_HW_INCLUDE_VALVE_2V025_H_ */
