/*
 * rtos.h
 *
 *  Created on: May 19, 2026
 *      Author: TEMP
 */

#ifndef SRC_COMMON_RTOS_DEF_H_
#define SRC_COMMON_RTOS_DEF_H_

#include "def.h"
#include "cmsis_os2.h"

#define _USE_HW_RTOS

#define _HW_DEF_RTOS_THREAD_PRI_MAIN          osPriorityNormal
#define _HW_DEF_RTOS_THREAD_MEM_MAIN          (2 * 1024)

#define _HW_DEF_RTOS_THREAD_PRI_LED           osPriorityLow
#define _HW_DEF_RTOS_THREAD_MEM_LED           (512)

#define _HW_DEF_RTOS_THREAD_PRI_MOTOR         osPriorityNormal
#define _HW_DEF_RTOS_THREAD_MEM_MOTOR         (1024)

#define _HW_DEF_RTOS_MSG_Q_MOTOR              8

#endif /* SRC_COMMON_RTOS_DEF_H_ */
