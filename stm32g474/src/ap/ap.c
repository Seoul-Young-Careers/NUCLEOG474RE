/*
 * ap.c
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */


#include "ap.h"

static void threadLed(void *argument);
static void threadMotor(void *argument);

static osMessageQueueId_t motor_msg_q;

void apInit(void)
{
  cliOpen(_DEF_UART1, 57600);

  if (osThreadNew(threadLed, NULL, rtosGetLedThreadAttr()) != NULL)
  {
    logPrintf("threadLed \t\t: OK\r\n");
  }
  else
  {
    logPrintf("threadLed \t\t: Fail\r\n");

    while (1)
    {
      delay(100);
    }
  }

  motor_msg_q = osMessageQueueNew(_HW_DEF_RTOS_MSG_Q_MOTOR,
                                  sizeof(rtos_motor_msg_t),
                                  rtosGetMotorMsgQAttr());

  if (motor_msg_q != NULL)
  {
    logPrintf("motorMsgQ \t\t: OK\r\n");
  }
  else
  {
    logPrintf("motorMsgQ \t\t: Fail\r\n");

    while (1)
    {
      delay(100);
    }
  }

  if (osThreadNew(threadMotor, NULL, rtosGetMotorThreadAttr()) != NULL)
  {
    logPrintf("threadMotor \t\t: OK\r\n");
  }
  else
  {
    logPrintf("threadMotor \t\t: Fail\r\n");

    while (1)
    {
      delay(100);
    }
  }

}

void apMain(void)
{

  while(1)
  {
    cliMain();
    delay(1);
  }
}

static void threadLed(void *argument)
{
  UNUSED(argument);

  while (1)
  {
    ledToggle(_DEF_LED1);
    osDelay(500);
  }
}

static void threadMotor(void *argument)
{
  rtos_motor_msg_t msg;

  UNUSED(argument);

  while (1)
  {
    if (osMessageQueueGet(motor_msg_q, &msg, NULL, osWaitForever) == osOK)
    {
      switch (msg.cmd)
      {
        case RTOS_MOTOR_CMD_MOVE_STEP:
          dm542MoveStep(msg.ch, msg.step, msg.pulse_delay_us);
          break;

        default:
          break;
      }
    }
  }
}
