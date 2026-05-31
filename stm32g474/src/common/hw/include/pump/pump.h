/*
 * pump.h
 *
 *  Created on: May 31, 2026
 *      Author: young
 */

#ifndef SRC_COMMON_HW_INCLUDE_PUMP_PUMP_H_
#define SRC_COMMON_HW_INCLUDE_PUMP_PUMP_H_

#include "hw_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _USE_PUMP

#define PUMP_LOCK_TIMEOUT_MS           100U

typedef struct
{
  bool is_ready;
  bool is_on;
} pump_data_t;

bool pumpInit(void);
bool pumpIsReady(void);

bool pumpOn(void);
bool pumpOff(void);
bool pumpSet(bool on);
bool pumpToggle(void);
bool pumpIsOn(void);
bool pumpReadData(pump_data_t *p_data);

#endif

#ifdef __cplusplus
}
#endif

#endif /* SRC_COMMON_HW_INCLUDE_PUMP_PUMP_H_ */
