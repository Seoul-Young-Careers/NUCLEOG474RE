/*
 * pump.c
 *
 *  Created on: May 31, 2026
 *      Author: young
 */

#include "pump/pump.h"

#ifdef _USE_PUMP

#ifdef _USE_HW_CLI
#include "cli.h"
static void cliPump(cli_args_t *args);
#endif

#define PUMP_PORT              GPIOB
#define PUMP_PIN               GPIO_PIN_5
#define PUMP_ON_STATE          GPIO_PIN_SET
#define PUMP_OFF_STATE         GPIO_PIN_RESET

static bool pump_is_ready = false;
static bool pump_is_on = false;

#ifdef _USE_HW_RTOS
static osMutexId_t pump_mutex = NULL;
static const osMutexAttr_t pump_mutex_attr =
{
  .name      = "pump",
  .attr_bits = osMutexRecursive | osMutexPrioInherit,
};
#endif

static bool pumpLock(void);
static void pumpUnlock(void);
static void pumpWritePin(bool on);

bool pumpInit(void)
{
  bool ret = true;
  GPIO_InitTypeDef GPIO_InitStruct = {0};

#ifdef _USE_HW_RTOS
  if(pump_mutex == NULL)
  {
    pump_mutex = osMutexNew(&pump_mutex_attr);
    if(pump_mutex == NULL)
    {
      ret = false;
    }
  }
#endif

  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Pin   = PUMP_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PUMP_PORT, &GPIO_InitStruct);

  pump_is_ready = true;
  pump_is_on = false;
  pumpWritePin(false);

#ifdef _USE_HW_CLI
  cliAdd("pump", cliPump);
#endif

  return ret;
}

bool pumpIsReady(void)
{
  bool ret;

  if(pumpLock() != true) return false;

  ret = pump_is_ready;

  pumpUnlock();

  return ret;
}

bool pumpOn(void)
{
  return pumpSet(true);
}

bool pumpOff(void)
{
  return pumpSet(false);
}

bool pumpSet(bool on)
{
  bool ret = false;

  if(pumpLock() != true) return false;

  do
  {
    if(pump_is_ready != true) break;

    pumpWritePin(on);
    pump_is_on = on;
    ret = true;
  } while(0);

  pumpUnlock();

  return ret;
}

bool pumpToggle(void)
{
  bool ret = false;

  if(pumpLock() != true) return false;

  do
  {
    if(pump_is_ready != true) break;

    pump_is_on = !pump_is_on;
    pumpWritePin(pump_is_on);
    ret = true;
  } while(0);

  pumpUnlock();

  return ret;
}

bool pumpIsOn(void)
{
  bool ret = false;

  if(pumpLock() != true) return false;

  if(pump_is_ready == true)
  {
    ret = pump_is_on;
  }

  pumpUnlock();

  return ret;
}

bool pumpReadData(pump_data_t *p_data)
{
  bool ret = false;

  if(pumpLock() != true) return false;

  do
  {
    if(p_data == NULL) break;

    p_data->is_ready = pump_is_ready;
    p_data->is_on    = pump_is_on;
    ret = true;
  } while(0);

  pumpUnlock();

  return ret;
}

static void pumpWritePin(bool on)
{
  HAL_GPIO_WritePin(PUMP_PORT,
                    PUMP_PIN,
                    on ? PUMP_ON_STATE : PUMP_OFF_STATE);
}

static bool pumpLock(void)
{
#ifdef _USE_HW_RTOS
  if(__get_IPSR() != 0U) return false;

  if((pump_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    return osMutexAcquire(pump_mutex, PUMP_LOCK_TIMEOUT_MS) == osOK;
  }
#endif

  return true;
}

static void pumpUnlock(void)
{
#ifdef _USE_HW_RTOS
  if((pump_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    (void)osMutexRelease(pump_mutex);
  }
#endif
}

#ifdef _USE_HW_CLI
static void cliPump(cli_args_t *args)
{
  bool ret = false;
  bool cmd_ret;

  if(args->argc == 1)
  {
    if(args->isStr(0, "show") == true)
    {
      cliPrintf("pump ready:%d on:%d\n", pumpIsReady(), pumpIsOn());
      ret = true;
    }

    if(args->isStr(0, "on") == true)
    {
      cmd_ret = pumpOn();
      cliPrintf("pump on : %s\n", cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "off") == true)
    {
      cmd_ret = pumpOff();
      cliPrintf("pump off : %s\n", cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "toggle") == true)
    {
      cmd_ret = pumpToggle();
      cliPrintf("pump toggle : %s (on:%d)\n",
                cmd_ret ? "OK" : "FAIL",
                pumpIsOn());
      ret = true;
    }

    if(args->isStr(0, "read") == true)
    {
      cliPrintf("pump read : on:%d\n", pumpIsOn());
      ret = true;
    }
  }

  if(args->argc == 2)
  {
    if(args->isStr(0, "set") == true)
    {
      bool on = (args->getData(1) != 0);

      cmd_ret = pumpSet(on);
      cliPrintf("pump set %d : %s\n",
                on ? 1 : 0,
                cmd_ret ? "OK" : "FAIL");
      ret = true;
    }
  }

  if(ret != true)
  {
    cliPrintf("pump show\n");
    cliPrintf("pump on\n");
    cliPrintf("pump off\n");
    cliPrintf("pump toggle\n");
    cliPrintf("pump read\n");
    cliPrintf("pump set 0:1\n");
  }
}
#endif

#endif
