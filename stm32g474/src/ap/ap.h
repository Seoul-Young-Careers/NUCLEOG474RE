/*
 * ap.h
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */

#ifndef SRC_AP_AP_H_
#define SRC_AP_AP_H_

#include "hw.h"

void apInit(void);
void apMain(void);

bool apMotorMoveStep(uint8_t ch, int32_t step, uint32_t pulse_delay_us);

#endif /* SRC_AP_AP_H_ */
