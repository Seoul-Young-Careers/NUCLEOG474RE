/*
 * app_sequence.h
 *
 *  Created on: May 29, 2026
 *      Author: young
 */

#ifndef SRC_AP_APP_SEQUENCE_H_
#define SRC_AP_APP_SEQUENCE_H_

#include "hw.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  APP_SEQUENCE_STATE_BOOT = 0,
  APP_SEQUENCE_STATE_HOMING,
  APP_SEQUENCE_STATE_IDLE_HOME,
  APP_SEQUENCE_STATE_MOVING_TO_END,
  APP_SEQUENCE_STATE_END_ACTION,
  APP_SEQUENCE_STATE_READY_SEQUENCE,
  APP_SEQUENCE_STATE_RUNNING_SEQUENCE,
  APP_SEQUENCE_STATE_MOVING_TO_HOME,
  APP_SEQUENCE_STATE_ERROR,
} app_sequence_state_t;

bool appSequenceInit(void);
void appSequenceProcess(void);
app_sequence_state_t appSequenceGetState(void);

#ifdef __cplusplus
}
#endif

#endif /* SRC_AP_APP_SEQUENCE_H_ */
