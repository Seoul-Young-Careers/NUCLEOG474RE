/*
 * sn04.c
 *
 *  Created on: May 22, 2026
 *      Author: young
 */

#include "sensor/sn04.h"

#ifdef _USE_SN04

#ifdef _USE_HW_CLI
#include "cli.h"
static void cliSn04(cli_args_t *args);
#endif

typedef struct
{
  GPIO_TypeDef  *port;
  uint32_t       pin;
  GPIO_PinState  on_state;
} sn04_pin_t;

typedef struct
{
  bool is_ready;
  bool is_detected;
} sn04_tbl_t;

/*
 * SN04-N : NPN open-collector proximity sensor.
 *  - Detection -> output line pulled LOW
 *  - No detection -> floating/HIGH (with internal pull-up)
 */
static const sn04_pin_t sn04_pin_tbl[SN04_MAX_CH] =
{
  {GPIOA, GPIO_PIN_8, GPIO_PIN_RESET},
  {GPIOB, GPIO_PIN_10, GPIO_PIN_RESET},
};

static sn04_tbl_t sn04_tbl[SN04_MAX_CH];

bool sn04Init(void)
{
  bool ret = true;
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;

  for(uint8_t i = 0; i < SN04_MAX_CH; i++)
  {
    GPIO_InitStruct.Pin = sn04_pin_tbl[i].pin;
    HAL_GPIO_Init(sn04_pin_tbl[i].port, &GPIO_InitStruct);

    sn04_tbl[i].is_ready    = true;
    sn04_tbl[i].is_detected = false;
  }

#ifdef _USE_HW_CLI
  cliAdd("sn04", cliSn04);
#endif

  return ret;
}

bool sn04IsReady(uint8_t ch)
{
  if(ch >= SN04_MAX_CH) return false;

  return sn04_tbl[ch].is_ready;
}

bool sn04Read(uint8_t ch)
{
  bool ret = false;

  if(ch >= SN04_MAX_CH) return false;
  if(sn04_tbl[ch].is_ready != true) return false;

  if(HAL_GPIO_ReadPin(sn04_pin_tbl[ch].port, sn04_pin_tbl[ch].pin) == sn04_pin_tbl[ch].on_state)
  {
    ret = true;
  }

  sn04_tbl[ch].is_detected = ret;

  return ret;
}

bool sn04IsDetected(uint8_t ch)
{
  return sn04Read(ch);
}

bool sn04ReadData(uint8_t ch, sn04_data_t *p_data)
{
  if(ch >= SN04_MAX_CH) return false;
  if(p_data == NULL) return false;

  p_data->is_ready    = sn04_tbl[ch].is_ready;
  p_data->is_detected = sn04Read(ch);

  return true;
}

#ifdef _USE_HW_CLI
static void cliSn04(cli_args_t *args)
{
  bool ret = false;

  if(args->argc == 1)
  {
    if(args->isStr(0, "show") == true)
    {
      while(cliKeepLoop())
      {
        for(uint8_t i = 0; i < SN04_MAX_CH; i++)
        {
          cliPrintf("sn04 %d ready:%d detected:%d\n",
                    i,
                    sn04IsReady(i),
                    sn04Read(i));
        }
        cliPrintf("\n");
        delay(100);
      }
      ret = true;
    }

    if(args->isStr(0, "info") == true)
    {
      for(uint8_t i = 0; i < SN04_MAX_CH; i++)
      {
        cliPrintf("sn04 %d ready:%d detected:%d\n",
                  i,
                  sn04IsReady(i),
                  sn04Read(i));
      }
      ret = true;
    }
  }

  if(args->argc == 2)
  {
    uint8_t ch = (uint8_t)args->getData(1);

    if(args->isStr(0, "read") == true)
    {
      cliPrintf("sn04 read %d : %d\n", ch, sn04Read(ch));
      ret = true;
    }
  }

  if(ret != true)
  {
    cliPrintf("sn04 show\n");
    cliPrintf("sn04 info\n");
    cliPrintf("sn04 read ch[0~%d]\n", SN04_MAX_CH - 1);
  }
}
#endif

#endif
