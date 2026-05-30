/*
 * 2v025.c
 *
 *  Created on: May 22, 2026
 *      Author: young
 */

#include "valve/2v025.h"

#ifdef _USE_2V025

#ifdef _USE_HW_CLI
#include "cli.h"
static void cliV025(cli_args_t *args);
#endif

typedef struct
{
  GPIO_TypeDef  *port;
  uint32_t       pin;
  GPIO_PinState  on_state;
  GPIO_PinState  off_state;
} v025_pin_t;

typedef struct
{
  bool is_ready;
  bool is_open;
} v025_tbl_t;

/*
 * 2V025 : 2-port / 2-position solenoid valve.
 *  - Drive HIGH -> valve OPEN (solenoid energized)
 *  - Drive LOW  -> valve CLOSED (default, spring return)
 */
static const v025_pin_t v025_pin_tbl[V025_MAX_CH] =
{
  {GPIOA, GPIO_PIN_9, GPIO_PIN_SET, GPIO_PIN_RESET},
  {GPIOC, GPIO_PIN_7, GPIO_PIN_SET, GPIO_PIN_RESET},

};

static v025_tbl_t v025_tbl[V025_MAX_CH];

#ifdef _USE_HW_RTOS
static osMutexId_t v025_mutex = NULL;
static const osMutexAttr_t v025_mutex_attr =
{
  .name      = "v025",
  .attr_bits = osMutexRecursive | osMutexPrioInherit,
};
#endif

static bool v025Lock(void);
static void v025Unlock(void);
static void v025WritePin(uint8_t ch, bool open)
{
  if(open == true)
  {
    HAL_GPIO_WritePin(v025_pin_tbl[ch].port,
                      v025_pin_tbl[ch].pin,
                      v025_pin_tbl[ch].on_state);
  }
  else
  {
    HAL_GPIO_WritePin(v025_pin_tbl[ch].port,
                      v025_pin_tbl[ch].pin,
                      v025_pin_tbl[ch].off_state);
  }
}

bool v025Init(void)
{
  bool ret = true;
  GPIO_InitTypeDef GPIO_InitStruct = {0};

#ifdef _USE_HW_RTOS
  if(v025_mutex == NULL)
  {
    v025_mutex = osMutexNew(&v025_mutex_attr);
    if(v025_mutex == NULL)
    {
      ret = false;
    }
  }
#endif

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  for(uint8_t i = 0; i < V025_MAX_CH; i++)
  {
    GPIO_InitStruct.Pin = v025_pin_tbl[i].pin;
    HAL_GPIO_Init(v025_pin_tbl[i].port, &GPIO_InitStruct);

    v025_tbl[i].is_ready = true;
    v025_tbl[i].is_open  = false;

    v025WritePin(i, false);
  }

#ifdef _USE_HW_CLI
  cliAdd("v025", cliV025);
#endif

  return ret;
}

bool v025IsReady(uint8_t ch)
{
  bool ret = false;

  if(v025Lock() != true) return false;

  if(ch < V025_MAX_CH)
  {
    ret = v025_tbl[ch].is_ready;
  }

  v025Unlock();

  return ret;
}

bool v025ValveOpen(uint8_t ch)
{
  return v025ValveSet(ch, true);
}

bool v025ValveClose(uint8_t ch)
{
  return v025ValveSet(ch, false);
}

bool v025ValveSet(uint8_t ch, bool open)
{
  bool ret = false;

  if(v025Lock() != true) return false;

  do
  {
    if(ch >= V025_MAX_CH) break;
    if(v025_tbl[ch].is_ready != true) break;

    v025WritePin(ch, open);
    v025_tbl[ch].is_open = open;
    ret = true;
  } while(0);

  v025Unlock();

  return ret;
}

bool v025ValveToggle(uint8_t ch)
{
  bool ret = false;

  if(v025Lock() != true) return false;

  do
  {
    if(ch >= V025_MAX_CH) break;
    if(v025_tbl[ch].is_ready != true) break;

    ret = v025ValveSet(ch, !v025_tbl[ch].is_open);
  } while(0);

  v025Unlock();

  return ret;
}

bool v025ValveIsOpen(uint8_t ch)
{
  bool ret = false;

  if(v025Lock() != true) return false;

  if((ch < V025_MAX_CH) && (v025_tbl[ch].is_ready == true))
  {
    ret = v025_tbl[ch].is_open;
  }

  v025Unlock();

  return ret;
}

bool v025ReadData(uint8_t ch, v025_data_t *p_data)
{
  bool ret = false;

  if(v025Lock() != true) return false;

  do
  {
    if(ch >= V025_MAX_CH) break;
    if(p_data == NULL) break;

    p_data->is_ready = v025_tbl[ch].is_ready;
    p_data->is_open  = v025_tbl[ch].is_open;

    ret = true;
  } while(0);

  v025Unlock();

  return ret;
}

static bool v025Lock(void)
{
#ifdef _USE_HW_RTOS
  if(__get_IPSR() != 0U) return false;

  if((v025_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    return osMutexAcquire(v025_mutex, V025_LOCK_TIMEOUT_MS) == osOK;
  }
#endif

  return true;
}

static void v025Unlock(void)
{
#ifdef _USE_HW_RTOS
  if((v025_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    (void)osMutexRelease(v025_mutex);
  }
#endif
}

#ifdef _USE_HW_CLI
static void cliV025(cli_args_t *args)
{
  bool ret = false;
  bool cmd_ret;
  uint8_t ch;

  if(args->argc == 1)
  {
    if(args->isStr(0, "show") == true)
    {
      for(uint8_t i = 0; i < V025_MAX_CH; i++)
      {
        cliPrintf("valve %d ready:%d open:%d\n",
                  i,
                  v025IsReady(i),
                  v025ValveIsOpen(i));
      }
      ret = true;
    }
  }

  if(args->argc == 2)
  {
    ch = (uint8_t)args->getData(1);

    if(args->isStr(0, "open") == true)
    {
      cmd_ret = v025ValveOpen(ch);
      cliPrintf("valve open %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "close") == true)
    {
      cmd_ret = v025ValveClose(ch);
      cliPrintf("valve close %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "toggle") == true)
    {
      cmd_ret = v025ValveToggle(ch);
      cliPrintf("valve toggle %d : %s (open:%d)\n",
                ch,
                cmd_ret ? "OK" : "FAIL",
                v025ValveIsOpen(ch));
      ret = true;
    }

    if(args->isStr(0, "read") == true)
    {
      cliPrintf("valve read %d : open:%d\n", ch, v025ValveIsOpen(ch));
      ret = true;
    }
  }

  if(args->argc == 3)
  {
    ch = (uint8_t)args->getData(1);

    if(args->isStr(0, "set") == true)
    {
      bool open_val = (args->getData(2) != 0);

      cmd_ret = v025ValveSet(ch, open_val);
      cliPrintf("valve set %d %d : %s\n",
                ch,
                open_val ? 1 : 0,
                cmd_ret ? "OK" : "FAIL");
      ret = true;
    }
  }

  if(ret != true)
  {
    cliPrintf("valve show\n");
    cliPrintf("valve open   ch[0~%d]\n", V025_MAX_CH - 1);
    cliPrintf("valve close  ch[0~%d]\n", V025_MAX_CH - 1);
    cliPrintf("valve toggle ch[0~%d]\n", V025_MAX_CH - 1);
    cliPrintf("valve read   ch[0~%d]\n", V025_MAX_CH - 1);
    cliPrintf("valve set    ch[0~%d] 0:1\n", V025_MAX_CH - 1);
  }
}
#endif

#endif
