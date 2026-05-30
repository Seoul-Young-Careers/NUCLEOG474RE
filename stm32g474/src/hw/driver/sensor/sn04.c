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
} sn04_pin_t;

typedef struct
{
  bool is_ready;
  bool is_detected;
} sn04_tbl_t;

/*
 * SN04-N : NPN open-collector proximity sensor.
 *  - Idle/no target -> internal pull-up makes the input HIGH
 *  - Detected      -> sensor output pulls the input LOW
 */
#define SN04_NPN_IDLE_PULL        GPIO_PULLUP
#define SN04_NPN_DETECTED_STATE   GPIO_PIN_RESET

static const sn04_pin_t sn04_pin_tbl[SN04_MAX_CH] =
{
  {GPIOA, GPIO_PIN_8},
  {GPIOB, GPIO_PIN_10},
};

static const IRQn_Type sn04_irq_tbl[SN04_MAX_CH] =
{
  EXTI9_5_IRQn,    /* PA8  -> EXTI line 8  */
  EXTI15_10_IRQn,  /* PB10 -> EXTI line 10 */
};

#define SN04_EXTI_IRQ_PRIO   5U

static sn04_tbl_t sn04_tbl[SN04_MAX_CH];
static volatile sn04_isr_cb_t sn04_isr_cb = NULL;

static bool sn04Lock(void);
static void sn04Unlock(void);
static GPIO_PinState sn04ReadRaw(uint8_t ch);
static bool sn04ReadDetected(uint8_t ch);

#ifdef _USE_HW_RTOS
static osMutexId_t sn04_mutex = NULL;
static const osMutexAttr_t sn04_mutex_attr =
{
  .name      = "sn04",
  .attr_bits = osMutexRecursive | osMutexPrioInherit,
};
#endif

bool sn04Init(void)
{
  bool ret = true;
  GPIO_InitTypeDef GPIO_InitStruct = {0};

#ifdef _USE_HW_RTOS
  if(sn04_mutex == NULL)
  {
    sn04_mutex = osMutexNew(&sn04_mutex_attr);
    if(sn04_mutex == NULL)
    {
      ret = false;
    }
  }
#endif

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* EXTI 양 edge 트리거 (NPN 센서: 감지 LOW / 해제 HIGH 모두 알림) */
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = SN04_NPN_IDLE_PULL;

  for(uint8_t i = 0; i < SN04_MAX_CH; i++)
  {
    GPIO_InitStruct.Pin = sn04_pin_tbl[i].pin;
    HAL_GPIO_Init(sn04_pin_tbl[i].port, &GPIO_InitStruct);

    sn04_tbl[i].is_ready    = true;
    sn04_tbl[i].is_detected = sn04ReadDetected(i);
  }

  /* 같은 IRQn을 두 번 호출해도 무방 (HAL이 중복 무시) */
  for(uint8_t i = 0; i < SN04_MAX_CH; i++)
  {
    HAL_NVIC_SetPriority(sn04_irq_tbl[i], SN04_EXTI_IRQ_PRIO, 0U);
    HAL_NVIC_EnableIRQ(sn04_irq_tbl[i]);
  }

#ifdef _USE_HW_CLI
  cliAdd("sn04", cliSn04);
#endif

  return ret;
}

bool sn04IsReady(uint8_t ch)
{
  bool ret = false;

  if(sn04Lock() != true) return false;

  if(ch < SN04_MAX_CH)
  {
    ret = sn04_tbl[ch].is_ready;
  }

  sn04Unlock();

  return ret;
}

bool sn04Read(uint8_t ch)
{
  bool ret = false;

  if(sn04Lock() != true) return false;

  do
  {
    if(ch >= SN04_MAX_CH) break;
    if(sn04_tbl[ch].is_ready != true) break;

    ret = sn04ReadDetected(ch);

    sn04_tbl[ch].is_detected = ret;
  } while(0);

  sn04Unlock();

  return ret;
}

bool sn04IsDetected(uint8_t ch)
{
  return sn04Read(ch);
}

bool sn04SetIsrCallback(sn04_isr_cb_t cb)
{
  sn04_isr_cb = cb;
  return true;
}

/* ISR-safe: 뮤텍스 미사용, HAL_GPIO_ReadPin은 단순 레지스터 read */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  for(uint8_t i = 0; i < SN04_MAX_CH; i++)
  {
    if(sn04_pin_tbl[i].pin != GPIO_Pin) continue;

    bool detected = sn04ReadDetected(i);

    sn04_tbl[i].is_detected = detected;

    sn04_isr_cb_t cb = sn04_isr_cb;
    if(cb != NULL)
    {
      cb((uint8_t)i, detected);
    }
    break;
  }
}

/*
 * EXTI IRQ 핸들러를 sn04 드라이버 안에서 직접 override.
 * (startup_stm32g474retx.s 의 weak symbol을 덮어씀)
 *   PA8  -> EXTI line 8  -> EXTI9_5_IRQHandler
 *   PB10 -> EXTI line 10 -> EXTI15_10_IRQHandler
 */
void EXTI9_5_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8);
}

void EXTI15_10_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);
}

bool sn04ReadData(uint8_t ch, sn04_data_t *p_data)
{
  bool ret = false;

  if(sn04Lock() != true) return false;

  do
  {
    if(ch >= SN04_MAX_CH) break;
    if(p_data == NULL) break;

    p_data->is_ready    = sn04_tbl[ch].is_ready;
    p_data->is_detected = sn04Read(ch);

    ret = true;
  } while(0);

  sn04Unlock();

  return ret;
}

static bool sn04Lock(void)
{
#ifdef _USE_HW_RTOS
  if(__get_IPSR() != 0U) return false;

  if((sn04_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    return osMutexAcquire(sn04_mutex, SN04_LOCK_TIMEOUT_MS) == osOK;
  }
#endif

  return true;
}

static void sn04Unlock(void)
{
#ifdef _USE_HW_RTOS
  if((sn04_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    (void)osMutexRelease(sn04_mutex);
  }
#endif
}

static GPIO_PinState sn04ReadRaw(uint8_t ch)
{
  return HAL_GPIO_ReadPin(sn04_pin_tbl[ch].port, sn04_pin_tbl[ch].pin);
}

static bool sn04ReadDetected(uint8_t ch)
{
  return sn04ReadRaw(ch) == SN04_NPN_DETECTED_STATE;
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
          cliPrintf("sn04 %d ready:%d raw:%d detected:%d\n",
                    i,
                    sn04IsReady(i),
                    sn04ReadRaw(i) == GPIO_PIN_SET ? 1 : 0,
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
        cliPrintf("sn04 %d ready:%d raw:%d detected:%d\n",
                  i,
                  sn04IsReady(i),
                  sn04ReadRaw(i) == GPIO_PIN_SET ? 1 : 0,
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
      if(ch < SN04_MAX_CH)
      {
        cliPrintf("sn04 read %d : raw:%d detected:%d\n",
                  ch,
                  sn04ReadRaw(ch) == GPIO_PIN_SET ? 1 : 0,
                  sn04Read(ch));
      }
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
