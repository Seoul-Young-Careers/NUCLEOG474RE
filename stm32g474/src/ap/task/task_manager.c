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
#include "task/task_sensor.h"
#include "task/task_sequence.h"
#include "task/task_servo.h"
#include "task/task_stepmotor.h"
#include "task/task_valve.h"

static bool taskManagerCheck(const char *p_name, bool result);

bool taskManagerInit(void)
{
  if(taskManagerCheck("threadLed", taskLedInit()) != true) return false;
  if(taskManagerCheck("appEvent", appEventInit()) != true) return false;
  if(taskManagerCheck("threadButton", taskButtonInit()) != true) return false;
  if(taskManagerCheck("threadSensor", taskSensorInit()) != true) return false;

#ifdef _USE_BTS7960
  if(taskManagerCheck("taskDcMotor", taskDcMotorInit()) != true) return false;
#endif

#ifdef _USE_DS3120MG
  if(taskManagerCheck("taskServo", taskServoInit()) != true) return false;
#endif

#ifdef _USE_2V025
  if(taskManagerCheck("taskValve", taskValveInit()) != true) return false;
#endif

#ifdef _USE_DM542
  if(taskStepMotorInit() != true) return false;
#endif

  if(taskManagerCheck("threadSequence", taskSequenceInit()) != true) return false;

  return true;
}

static bool taskManagerCheck(const char *p_name, bool result)
{
  logPrintf("%s \t\t: %s\r\n", p_name, result ? "OK" : "Fail");

  return result;
}
