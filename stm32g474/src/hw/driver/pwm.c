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
  uint32_t tim_channel;

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
    .p_tim          = &htim3,
    .tim_channel    = TIM_CHANNEL_1,

    .is_open        = false,
    .is_busy        = false,
    .is_count_mode  = false,
    .remain_count   = 0,
  },
};

static pwm_cfg_t pwm_cfg[PWM_MAX_CH] =
{
  { 79, 999, 5 },
};

#ifdef _USE_HW_CLI
//static void cliTimPwm(cli_args_t *args);
#endif

bool pwmInit(void)
{
  for(uint8_t i = 0; i < PWM_MAX_CH; i++)
  {
    pwm_tbl[i].is_open       = false;
    pwm_tbl[i].is_busy       = false;
    pwm_tbl[i].is_count_mode = false;
    pwm_tbl[i].remain_count  = 0;

    pwmOpen(i);
  }

#ifdef _USE_HW_CLI
//  cliAdd("pwm", cliPwm);
#endif

  return true;
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
    switch(ch)
    {
      case _DEF_PWM1:
        __HAL_RCC_TIM16_CLK_ENABLE();

        p_tim->Instance               = TIM3;
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

        if (HAL_TIMEx_MasterConfigSynchronization(p_tim, &sMasterConfig) != HAL_OK)
        {
            p_tim->Instance = NULL;
            return false;
        }

        sConfigOC.OCMode 			= TIM_OCMODE_PWM1;
        sConfigOC.Pulse 			= pwm_cfg[ch].pulse;
        sConfigOC.OCPolarity 		= TIM_OCPOLARITY_HIGH;
        sConfigOC.OCFastMode 		= TIM_OCFAST_DISABLE;

        if (HAL_TIM_PWM_ConfigChannel(p_tim, &sConfigOC, pwm_tbl[ch].tim_channel) != HAL_OK)
        {
          Error_Handler();
        }

        pwmSetGpioMode(ch,GPIO_MODE_AF_OD);

        break;
    }
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

bool PwmStart(uint8_t ch)
{
  bool ret = true;
  TIM_HandleTypeDef *p_tim;

  if(ch >= PWM_MAX_CH)  return false;
  if(pwm_tbl[ch].is_open != true) return false;

  p_tim = pwm_tbl[ch].p_tim;
  __HAL_TIM_SET_COUNTER(p_tim, 0);

  if(HAL_TIM_PWM_Start(p_tim, pwm_tbl[ch].tim_channel) != HAL_OK) return false;

  pwm_tbl[ch].is_busy = true;
  return ret;
}

bool pwmStop(uint8_t ch)
{
  bool ret = true;
  TIM_HandleTypeDef *p_tim;

  if(ch >= PWM_MAX_CH) return false;
  if(pwm_tbl[ch].is_open != true) return false;

  p_tim = pwm_tbl[ch].p_tim;

  if(HAL_TIM_PWM_Stop(p_tim, pwm_tbl[ch].tim_channel) != HAL_OK) return false;

  pwm_tbl[ch].is_busy = false;

  return ret;
}

bool pwmSetGpioMode(uint8_t ch, uint32_t mode)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if(ch >= PWM_MAX_CH) return false;

  switch(ch)
  {
    case _DEF_PWM1:

      GPIO_InitStruct.Pin       = GPIO_PIN_6;
      GPIO_InitStruct.Mode      = mode;
      GPIO_InitStruct.Pull      = GPIO_NOPULL;
      GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
      GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;

      HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
      break;
  }

  return true;
}

bool PwmSetPrescaler(uint8_t ch, uint32_t prescaler)
{
  TIM_HandleTypeDef *p_tim;

  if(ch >= PWM_MAX_CH)  return false;
  if(prescaler > 0xFFFFU) return false;

  p_tim = pwm_tbl[ch].p_tim;

  if(p_tim->Instance == NULL) return false;

  p_tim->Init.Prescaler = prescaler;
  pwm_cfg[ch].prescaler = prescaler;

  __HAL_TIM_SET_PRESCALER(p_tim, prescaler);
  HAL_TIM_GenerateEvent(p_tim, TIM_EVENTSOURCE_UPDATE);

  return true;
}

bool PwmSetPeriod(uint8_t ch,uint32_t period)
{
  TIM_HandleTypeDef *p_tim;

  if(ch >= PWM_MAX_CH) return false;
  if(period > 0xFFFFU)  return false;

  p_tim = pwm_tbl[ch].p_tim;

  if(p_tim->Instance == NULL) return false;

  p_tim->Init.Period = period;
  pwm_cfg[ch].period = period;

  __HAL_TIM_SET_AUTORELOAD(p_tim, period);
  __HAL_TIM_SET_COUNTER(p_tim, 0);
  HAL_TIM_GenerateEvent(p_tim, TIM_EVENTSOURCE_UPDATE);

  return true;
}

bool PwmSetPulse(uint8_t ch,uint32_t pulse)
{
  TIM_HandleTypeDef *p_tim;
  TIM_OC_InitTypeDef sConfigOC = {0};

  if(ch >= PWM_MAX_CH)  return false;

  p_tim = pwm_tbl[ch].p_tim;
  pwm_cfg[ch].pulse = pulse;

  if(p_tim->Instance == NULL) return false;
switch(ch)
{
  case _DEF_PWM1:
  sConfigOC.OCMode       = TIM_OCMODE_PWM1;
  sConfigOC.Pulse        = pulse;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  if(HAL_TIM_PWM_ConfigChannel(p_tim, &sConfigOC, pwm_tbl[ch].tim_channel) != HAL_OK)
  {
    return false;
  }

  __HAL_TIM_SET_COMPARE(p_tim, pwm_tbl[ch].tim_channel, pulse);
}

  return true;
}



#endif
