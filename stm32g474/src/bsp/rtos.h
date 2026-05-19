/*
 * rtos.h
 *
 *  Created on: May 18, 2026
 *      Author: TEMP
 */

#ifndef SRC_BSP_RTOS_H_
#define SRC_BSP_RTOS_H_

#include "def.h"
#include "rtos_def.h"

bool rtosInit(void);

const osThreadAttr_t *rtosGetMainThreadAttr(void);
const osThreadAttr_t *rtosGetLedThreadAttr(void);
const osThreadAttr_t *rtosGetMotorThreadAttr(void);

#endif /* SRC_BSP_RTOS_H_ */
