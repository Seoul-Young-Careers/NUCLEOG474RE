/*
 * tim_pwm.c
 *
 *  Created on: May 14, 2026
 *      Author: young
 */

#include "pwm.h"

#ifdef _USE_PWM

#ifdef _USE_HW_CLI
#include "cli.h"
#endif

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

typedef struct
{
  TIM_HandleTypeDef *p_tim;
  TIM_TypeDef *instance;
  uint32_t tim_channel;
  IRQn_Type irq_num;

  bool is_open;
  bool is_busy;
  bool is_count_mode;
  uint32_t remain_count;
} pwm_tbl_t;

typedef struct
{
  uint32_t prescaler;
  uint32_t period;
  uint32_t pulse;
} pwm_cfg_t;

static pwm_tbl_t pwm_tbl[PWM_MAX_CH] =
{
  {
    .p_tim          = &htim2,
    .instance       = TIM2,
    .tim_channel    = TIM_CHANNEL_1,
    .irq_num        = TIM2_IRQn,

    .is_open        = false,
    .is_busy        = false,
    .is_count_mode  = false,
    .remain_count   = 0,
  },
  {
    .p_tim          = &htim3,
    .instance       = TIM3,
    .tim_channel    = TIM_CHANNEL_1,
    .irq_num        = TIM3_IRQn,

    .is_open        = false,
    .is_busy        = false,
    .is_count_mode  = false,
    .remain_count   = 0,
  },
  {
    .p_tim          = &htim4,
    .instance       = TIM4,
    .tim_channel    = TIM_CHANNEL_1,
    .irq_num        = TIM4_IRQn,

    .is_open        = false,
    .is_busy        = false,
    .is_count_mode  = false,
    .remain_count   = 0,
  },
};

static pwm_cfg_t pwm_cfg[PWM_MAX_CH] =
{
  { 79, 999, 5 },
  { 79, 999, 5 },
  { 79, 999, 5 },
};

#ifdef _USE_HW_CLI
static void cliPwm(cli_args_t *args);
#endif

static bool pwmEnableTimerClock(uint8_t ch);

bool pwmInit(void)
{
  bool ret = true;

  for(uint8_t i = 0; i < PWM_MAX_CH; i++)
  {
    pwm_tbl[i].is_open       = false;
    pwm_tbl[i].is_busy       = false;
    pwm_tbl[i].is_count_mode = false;
    pwm_tbl[i].remain_count  = 0;

    if(pwmOpen(i) != true)
    {
      ret = false;
    }
  }

#ifdef _USE_HW_CLI
  cliAdd("pwm", cliPwm);
#endif

  return ret;
}

bool pwmOpen(uint8_t ch)
{
  TIM_HandleTypeDef *p_tim;
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  if(ch >= PWM_MAX_CH)
  {
    return false;
  }

  p_tim = pwm_tbl[ch].p_tim;

  if(p_tim->Instance == NULL)
  {
    if(pwmEnableTimerClock(ch) != true)
    {
      return false;
    }

    p_tim->Instance               = pwm_tbl[ch].instance;
    p_tim->Init.Prescaler         = pwm_cfg[ch].prescaler;
    p_tim->Init.CounterMode       = TIM_COUNTERMODE_UP;
    p_tim->Init.Period            = pwm_cfg[ch].period;
    p_tim->Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    p_tim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if(HAL_TIM_PWM_Init(p_tim) != HAL_OK)
    {
      p_tim->Instance = NULL;
      return false;
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    if(HAL_TIMEx_MasterConfigSynchronization(p_tim, &sMasterConfig) != HAL_OK)
    {
      p_tim->Instance = NULL;
      return false;
    }

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = pwm_cfg[ch].pulse;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if(HAL_TIM_PWM_ConfigChannel(p_tim, &sConfigOC, pwm_tbl[ch].tim_channel) != HAL_OK)
    {
      p_tim->Instance = NULL;
      return false;
    }

    pwmSetGpioMode(ch, GPIO_MODE_AF_PP);
    HAL_NVIC_SetPriority(pwm_tbl[ch].irq_num, 5, 0);
    HAL_NVIC_EnableIRQ(pwm_tbl[ch].irq_num);
  }

  pwm_tbl[ch].is_open = true;

  return true;
}

bool pwmIsOpen(uint8_t ch)
{
  if(ch >= PWM_MAX_CH)  return false;

  return pwm_tbl[ch].is_open;
}

bool pwmIsBusy(uint8_t ch)
{
  if(ch >= PWM_MAX_CH)  return false;

  return pwm_tbl[ch].is_busy;
}

bool pwmStart(uint8_t ch)
{
  TIM_HandleTypeDef *p_tim;

  if(ch >= PWM_MAX_CH) return false;
  if(pwm_tbl[ch].is_open != true) return false;

  p_tim = pwm_tbl[ch].p_tim;
  __HAL_TIM_SET_COUNTER(p_tim, 0);

  if(HAL_TIM_PWM_Start(p_tim, pwm_tbl[ch].tim_channel) != HAL_OK) return false;

  pwm_tbl[ch].is_busy = true;

  return true;
}

bool pwmStop(uint8_t ch)
{
  TIM_HandleTypeDef *p_tim;

  if(ch >= PWM_MAX_CH) return false;
  if(pwm_tbl[ch].is_open != true) return false;

  p_tim = pwm_tbl[ch].p_tim;

  if(HAL_TIM_PWM_Stop(p_tim, pwm_tbl[ch].tim_channel) != HAL_OK) return false;

  pwm_tbl[ch].is_busy = false;

  return true;
}

bool pwmSetGpioMode(uint8_t ch, uint32_t mode)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if(ch >= PWM_MAX_CH) return false;

  GPIO_InitStruct.Mode  = mode;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  switch(ch)
  {
    case _DEF_PWM1:
      __HAL_RCC_GPIOA_CLK_ENABLE();
      GPIO_InitStruct.Pin       = GPIO_PIN_0;
      GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
      HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
      break;

    case _DEF_PWM2:
      __HAL_RCC_GPIOA_CLK_ENABLE();
      GPIO_InitStruct.Pin       = GPIO_PIN_6;
      GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
      HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
      break;

    case _DEF_PWM3:
      __HAL_RCC_GPIOB_CLK_ENABLE();
      GPIO_InitStruct.Pin       = GPIO_PIN_6;
      GPIO_InitStruct.Alternate = GPIO_AF2_TIM4;
      HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
      break;

    default:
      return false;
  }

  return true;
}

bool pwmSetPrescaler(uint8_t ch, uint32_t prescaler)
{
  TIM_HandleTypeDef *p_tim;

  if(ch >= PWM_MAX_CH) return false;
  if(prescaler > 0xFFFFU) return false;

  p_tim = pwm_tbl[ch].p_tim;

  if(p_tim->Instance == NULL) return false;

  p_tim->Init.Prescaler = prescaler;
  pwm_cfg[ch].prescaler = prescaler;

  __HAL_TIM_SET_PRESCALER(p_tim, prescaler);
  HAL_TIM_GenerateEvent(p_tim, TIM_EVENTSOURCE_UPDATE);

  return true;
}

bool pwmSetPeriod(uint8_t ch, uint32_t period)
{
  TIM_HandleTypeDef *p_tim;

  if(ch >= PWM_MAX_CH) return false;
  if(period > 0xFFFFU) return false;

  p_tim = pwm_tbl[ch].p_tim;

  if(p_tim->Instance == NULL) return false;

  p_tim->Init.Period = period;
  pwm_cfg[ch].period = period;

  __HAL_TIM_SET_AUTORELOAD(p_tim, period);
  __HAL_TIM_SET_COUNTER(p_tim, 0);
  HAL_TIM_GenerateEvent(p_tim, TIM_EVENTSOURCE_UPDATE);

  return true;
}

bool pwmSetPulse(uint8_t ch, uint32_t pulse)
{
  TIM_HandleTypeDef *p_tim;
  TIM_OC_InitTypeDef sConfigOC = {0};

  if(ch >= PWM_MAX_CH) return false;
  if(pulse > pwm_cfg[ch].period) return false;

  p_tim = pwm_tbl[ch].p_tim;

  if(p_tim->Instance == NULL) return false;

  pwm_cfg[ch].pulse = pulse;

  sConfigOC.OCMode     = TIM_OCMODE_PWM1;
  sConfigOC.Pulse      = pulse;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  if(HAL_TIM_PWM_ConfigChannel(p_tim, &sConfigOC, pwm_tbl[ch].tim_channel) != HAL_OK)
  {
    return false;
  }

  __HAL_TIM_SET_COMPARE(p_tim, pwm_tbl[ch].tim_channel, pulse);

  return true;
}

static bool pwmEnableTimerClock(uint8_t ch)
{
  switch(ch)
  {
    case _DEF_PWM1:
      __HAL_RCC_TIM2_CLK_ENABLE();
      return true;

    case _DEF_PWM2:
      __HAL_RCC_TIM3_CLK_ENABLE();
      return true;

    case _DEF_PWM3:
      __HAL_RCC_TIM4_CLK_ENABLE();
      return true;
  }

  return false;
}

#ifdef _USE_HW_CLI
static void cliPwm(cli_args_t *args)
{
  bool ret = false;

  if(args->argc == 1 && args->isStr(0, "show") == true)
  {
    for(uint8_t i = 0; i < PWM_MAX_CH; i++)
    {
      uint32_t duty = 0;

      if(pwm_cfg[i].period > 0)
      {
        duty = (pwm_cfg[i].pulse * 100U) / (pwm_cfg[i].period + 1U);
      }

      cliPrintf("pwm %d open:%d busy:%d psc:%lu period:%lu pulse:%lu duty:%lu%%\n",
                i,
                pwm_tbl[i].is_open,
                pwm_tbl[i].is_busy,
                pwm_cfg[i].prescaler,
                pwm_cfg[i].period,
                pwm_cfg[i].pulse,
                duty);
    }

    ret = true;
  }

  if(args->argc == 2 && args->isStr(0, "start") == true)
  {
    uint8_t ch = (uint8_t)args->getData(1);
    bool cmd_ret = pwmStart(ch);

    cliPrintf("pwm start %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
    ret = true;
  }

  if(args->argc == 2 && args->isStr(0, "stop") == true)
  {
    uint8_t ch = (uint8_t)args->getData(1);
    bool cmd_ret = pwmStop(ch);

    cliPrintf("pwm stop %d : %s\n", ch, cmd_ret ? "OK" : "FAIL");
    ret = true;
  }

  if(args->argc == 3 && args->isStr(0, "prescaler") == true)
  {
    uint8_t ch = (uint8_t)args->getData(1);
    uint32_t prescaler = (uint32_t)args->getData(2);
    bool cmd_ret = pwmSetPrescaler(ch, prescaler);

    cliPrintf("pwm prescaler %d %lu : %s\n", ch, prescaler, cmd_ret ? "OK" : "FAIL");
    ret = true;
  }

  if(args->argc == 3 && args->isStr(0, "period") == true)
  {
    uint8_t ch = (uint8_t)args->getData(1);
    uint32_t period = (uint32_t)args->getData(2);
    bool cmd_ret = pwmSetPeriod(ch, period);

    cliPrintf("pwm period %d %lu : %s\n", ch, period, cmd_ret ? "OK" : "FAIL");
    ret = true;
  }

  if(args->argc == 3 && args->isStr(0, "pulse") == true)
  {
    uint8_t ch = (uint8_t)args->getData(1);
    uint32_t pulse = (uint32_t)args->getData(2);
    bool cmd_ret = pwmSetPulse(ch, pulse);

    cliPrintf("pwm pulse %d %lu : %s\n", ch, pulse, cmd_ret ? "OK" : "FAIL");
    ret = true;
  }

  if(args->argc == 3 && args->isStr(0, "duty") == true)
  {
    uint8_t ch = (uint8_t)args->getData(1);
    uint32_t duty = (uint32_t)args->getData(2);
    bool cmd_ret = false;

    if(ch < PWM_MAX_CH && duty <= 100U)
    {
      uint32_t pulse = ((pwm_cfg[ch].period + 1U) * duty) / 100U;

      if(pulse > 0)
      {
        pulse--;
      }

      cmd_ret = pwmSetPulse(ch, pulse);
    }

    cliPrintf("pwm duty %d %lu : %s\n", ch, duty, cmd_ret ? "OK" : "FAIL");
    ret = true;
  }

  if(ret != true)
  {
    cliPrintf("pwm show\n");
    cliPrintf("pwm start ch[0~%d]\n", PWM_MAX_CH - 1);
    cliPrintf("pwm stop ch[0~%d]\n", PWM_MAX_CH - 1);
    cliPrintf("pwm prescaler ch[0~%d] value\n", PWM_MAX_CH - 1);
    cliPrintf("pwm period ch[0~%d] value\n", PWM_MAX_CH - 1);
    cliPrintf("pwm pulse ch[0~%d] value\n", PWM_MAX_CH - 1);
    cliPrintf("pwm duty ch[0~%d] percent[0~100]\n", PWM_MAX_CH - 1);
  }
}
#endif

#endif
