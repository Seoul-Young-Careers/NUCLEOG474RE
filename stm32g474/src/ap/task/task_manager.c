/*
 * task_manager.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/task_manager.h"

#include "task/app_event.h"
#include "task/task_button.h"
#include "task/task_dcmotor.h"
#include "task/task_led.h"
#include "task/task_pump.h"
#include "task/task_sensor.h"
#include "task/task_sequence.h"
#include "task/task_servo.h"
#include "task/task_stepmotor.h"
#include "task/task_valve.h"

bool taskManagerInit(void)
{
  if(taskLedInit() != true) return false;
  if(appEventInit() != true) return false;
  if(taskButtonInit() != true) return false;
  if(taskSensorInit() != true) return false;

#ifdef _USE_BTS7960
  if(taskDcMotorInit() != true) return false;
#endif

#ifdef _USE_DS3120MG
  if(taskServoInit() != true) return false;
#endif

#ifdef _USE_2V025
  if(taskValveInit() != true) return false;
#endif

#ifdef _USE_PUMP
  if(taskPumpInit() != true) return false;
#endif

#ifdef _USE_DM542
  if(taskStepMotorInit() != true) return false;
#endif

  if(taskSequenceInit() != true) return false;

  return true;
}
