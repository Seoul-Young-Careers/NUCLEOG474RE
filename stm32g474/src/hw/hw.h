/*
 * hw.h
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */

#ifndef SRC_HW_HW_H_
#define SRC_HW_HW_H_


#include <servomotor/ds3120mg.h>
#include <stepmotor/dm542.h>
#include <motordriver/bts7960.h>
#include "hw_def.h"

#include "led.h"
#include "uart.h"
#include "cli.h"
#include "button.h"
#include "gpio.h"
#include "pwm.h"
#include "loadcell/hx711.h"
#include "sensor/sn04.h"
#include "valve/2v025.h"

void hwInit(void);


#endif /* SRC_HW_HW_H_ */
