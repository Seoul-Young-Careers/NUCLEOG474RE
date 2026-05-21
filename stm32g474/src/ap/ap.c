/*
 * ap.c
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */


#include "ap.h"

static void threadLed(void *argument);
static void threadMotor(void *argument);

#ifdef _USE_HW_CLI
static void cliMotor(cli_args_t *args);
#endif

#ifdef _USE_HX711
static void threadLoadcell(void *argument);
#endif

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

#ifdef _USE_HW_CLI
  cliAdd("motor", cliMotor);
#endif

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

bool apMotorMoveStep(uint8_t ch, int32_t step, uint32_t pulse_delay_us)
{
  rtos_motor_msg_t msg;

  if(motor_msg_q == NULL) return false;
  if(pulse_delay_us == 0U) return false;

  msg.cmd            = RTOS_MOTOR_CMD_MOVE_STEP;
  msg.ch             = ch;
  msg.step           = step;
  msg.pulse_delay_us = pulse_delay_us;

  return osMessageQueuePut(motor_msg_q, &msg, 0U, 10U) == osOK;
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
    delay(500);
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

#ifdef _USE_HW_CLI
static void cliMotor(cli_args_t *args)
{
  bool ret = false;
  bool cmd_ret;
  uint8_t ch;
  int32_t step;
  uint32_t value;

  if(args->argc == 1)
  {
    if(args->isStr(0, "show") == true)
    {
      cliPrintf("motor queue count:%lu space:%lu\n",
                (unsigned long)osMessageQueueGetCount(motor_msg_q),
                (unsigned long)osMessageQueueGetSpace(motor_msg_q));

      for(uint8_t i = 0; i < DM542_MAX_CH; i++)
      {
        cliPrintf("motor %d open:%d busy:%d\n",
                  i,
                  dm542IsOpen(i),
                  dm542IsBusy(i));
      }

      ret = true;
    }
  }

  if(args->argc == 3)
  {
    ch   = (uint8_t)args->getData(1);
    step = args->getData(2);

    if(args->isStr(0, "run") == true)
    {
      cmd_ret = apMotorMoveStep(ch, step, 1000U);
      cliPrintf("motor run %d %ld : %s\n",
                ch,
                (long)step,
                cmd_ret ? "QUEUED" : "FAIL");
      ret = true;
    }
  }

  if(args->argc == 4)
  {
    ch    = (uint8_t)args->getData(1);
    step  = args->getData(2);
    value = (uint32_t)args->getData(3);

    if(args->isStr(0, "move") == true)
    {
      cmd_ret = apMotorMoveStep(ch, step, value);
      cliPrintf("motor move %d %ld %luus : %s\n",
                ch,
                (long)step,
                value,
                cmd_ret ? "QUEUED" : "FAIL");
      ret = true;
    }
  }

  if(ret != true)
  {
    cliPrintf("motor show\n");
    cliPrintf("motor run  ch[0~%d] step\n", DM542_MAX_CH - 1);
    cliPrintf("motor move ch[0~%d] step pulse_delay_us\n", DM542_MAX_CH - 1);
  }
}
#endif

#ifdef _USE_HX711
static void threadLoadcell(void *argument)
{
  float gram;

  UNUSED(argument);

  loadcellOpen(0);
  loadcellTare(0, 20);
  loadcellSetScale(0, 1.0f);

  while (1)
  {
    if (loadcellReadGram(0, &gram) == true)
    {
      // lcdClear();
      // lcdPrintf(0, 0, "Weight");
      // lcdPrintf(0, 20, "%.1f g", gram);
    }
    else
    {
      // lcdPrintf(0, 0, "Loadcell Error");
    }

    osDelay(100);
  }
}
#endif

