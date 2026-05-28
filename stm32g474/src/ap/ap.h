/*
 * ap.h
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */

#ifndef SRC_AP_AP_H_
#define SRC_AP_AP_H_

#include "hw.h"
#include "task/task_dcmotor.h"
#include "task/task_servo.h"
#include "task/task_stepmotor.h"
#include "task/task_valve.h"

void apInit(void);
void apMain(void);

#endif /* SRC_AP_AP_H_ */
