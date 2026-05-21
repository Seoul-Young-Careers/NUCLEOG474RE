/*
 * hw.h
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */

#ifndef SRC_HW_HW_H_
#define SRC_HW_HW_H_


#include "hw_def.h"

#include "led.h"
#include "uart.h"
#include "cli.h"
#include "button.h"
#include "gpio.h"
#include "pwm.h"
#include "dm542/dm542.h"
#include "loadcell/hx711.h"
#include "submotor/ds3120mg.h"

void hwInit(void);


#endif /* SRC_HW_HW_H_ */
