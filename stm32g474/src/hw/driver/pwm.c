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

#define od 			GPIO_MODE_AF_OD
#define pp			GPIO_MODE_AF_PP

typedef struct
{
  TIM_HandleTypeDef *p_tim;
  uint32_t channel;

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
    .channel        = TIM_CHANNEL_1,

    .is_open        = false,
    .is_busy        = false,
    .is_count_mode  = false,
    .remain_count   = 0,
  },
  {
    .p_tim          = &htim3,
    .channel        = TIM_CHANNEL_1,

    .is_open        = false,
    .is_busy        = false,
    .is_count_mode  = false,
    .remain_count   = 0,
  },
  {
    .p_tim          = &htim4,
    .channel        = TIM_CHANNEL_1,

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
	{ 169, 3332, 1500 },
};

#ifdef _USE_HW_CLI
static void cliPwm(cli_args_t *args);
#endif

bool pwmInit(void)
{
  bool ret = true;

  for(uint8_t i = 0; i < PWM_MAX_CH; i++)
  {
    pwm_tbl[i].is_open       = false;
    pwm_tbl[i].is_busy       = false;
    pwm_tbl[i].is_count_mode = false;
    pwm_tbl[i].remain_count  = 0;
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

  if(ch >= PWM_MAX_CH)		return false;
  if(pwm_tbl[ch].is_open == true) return true;

  p_tim = pwm_tbl[ch].p_tim;

  switch(ch)
  {
  case _DEF_PWM1 :
  	p_tim->Instance 			  			= TIM2;
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

    if(HAL_TIM_PWM_ConfigChannel(p_tim, &sConfigOC, pwm_tbl[ch].channel) != HAL_OK)
    {
      p_tim->Instance = NULL;
      return false;
    }

    pwmSetGpioMode(ch, od);

  pwm_tbl[ch].is_open = true;
  break;
  case _DEF_PWM2:
		p_tim->Instance 			  				= TIM3;
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

	    if(HAL_TIM_PWM_ConfigChannel(p_tim, &sConfigOC, pwm_tbl[ch].channel) != HAL_OK)
	    {
	      p_tim->Instance = NULL;
	      return false;
	    }

	    pwmSetGpioMode(ch, od);

	  pwm_tbl[ch].is_open = true;
	  break;
  case _DEF_PWM3:
		p_tim->Instance 			  				= TIM4;
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

	    if(HAL_TIM_PWM_ConfigChannel(p_tim, &sConfigOC, pwm_tbl[ch].channel) != HAL_OK)
	    {
	      p_tim->Instance = NULL;
	      return false;
	    }

	    pwmSetGpioMode(ch, pp);

	  pwm_tbl[ch].is_open = true;
	  break;
  }

  return true;
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* tim_pwmHandle)
{

  if(tim_pwmHandle->Instance==TIM2)
  {
  /* USER CODE BEGIN TIM2_MspInit 0 */

  /* USER CODE END TIM2_MspInit 0 */
    /* TIM2 clock enable */
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* TIM2 interrupt Init */
    HAL_NVIC_SetPriority(TIM2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
  /* USER CODE BEGIN TIM2_MspInit 1 */

  /* USER CODE END TIM2_MspInit 1 */
  }
  else if(tim_pwmHandle->Instance==TIM3)
  {
  /* USER CODE BEGIN TIM3_MspInit 0 */

  /* USER CODE END TIM3_MspInit 0 */
    /* TIM3 clock enable */
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* TIM3 interrupt Init */
    HAL_NVIC_SetPriority(TIM3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
  /* USER CODE BEGIN TIM3_MspInit 1 */

  /* USER CODE END TIM3_MspInit 1 */
  }
  else if(tim_pwmHandle->Instance==TIM4)
  {
  /* USER CODE BEGIN TIM4_MspInit 0 */

  /* USER CODE END TIM4_MspInit 0 */
    /* TIM4 clock enable */
    __HAL_RCC_TIM4_CLK_ENABLE();

    /* TIM4 interrupt Init */
    HAL_NVIC_SetPriority(TIM4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
  /* USER CODE BEGIN TIM4_MspInit 1 */

  /* USER CODE END TIM4_MspInit 1 */
  }
}

void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef* tim_pwmHandle)
{

  if(tim_pwmHandle->Instance==TIM2)
  {
  /* USER CODE BEGIN TIM2_MspDeInit 0 */

  /* USER CODE END TIM2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_TIM2_CLK_DISABLE();

    /* TIM2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(TIM2_IRQn);
  /* USER CODE BEGIN TIM2_MspDeInit 1 */

  /* USER CODE END TIM2_MspDeInit 1 */
  }
  else if(tim_pwmHandle->Instance==TIM3)
  {
  /* USER CODE BEGIN TIM3_MspDeInit 0 */

  /* USER CODE END TIM3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_TIM3_CLK_DISABLE();

    /* TIM3 interrupt Deinit */
    HAL_NVIC_DisableIRQ(TIM3_IRQn);
  /* USER CODE BEGIN TIM3_MspDeInit 1 */

  /* USER CODE END TIM3_MspDeInit 1 */
  }
  else if(tim_pwmHandle->Instance==TIM4)
  {
  /* USER CODE BEGIN TIM4_MspDeInit 0 */

  /* USER CODE END TIM4_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_TIM4_CLK_DISABLE();

    /* TIM4 interrupt Deinit */
    HAL_NVIC_DisableIRQ(TIM4_IRQn);
  /* USER CODE BEGIN TIM4_MspDeInit 1 */

  /* USER CODE END TIM4_MspDeInit 1 */
  }
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

  if(HAL_TIM_PWM_Start(p_tim, pwm_tbl[ch].channel) != HAL_OK) return false;

  pwm_tbl[ch].is_busy = true;

  return true;
}

bool pwmStop(uint8_t ch)
{
  TIM_HandleTypeDef *p_tim;

  if(ch >= PWM_MAX_CH) return false;
  if(pwm_tbl[ch].is_open != true) return false;

  p_tim = pwm_tbl[ch].p_tim;

  if(HAL_TIM_PWM_Stop(p_tim, pwm_tbl[ch].channel) != HAL_OK) return false;

  pwm_tbl[ch].is_busy = false;

  return true;
}
bool pwmRunUs(uint8_t ch, uint32_t time_us)
{
  bool ret;

  if(ch >= PWM_MAX_CH) return false;
  if(time_us == 0U) return false;

  ret = pwmStart(ch);
  if(ret != true) return false;

  delayUs(time_us);

  return pwmStop(ch);
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
      GPIO_InitStruct.Pin       = GPIO_PIN_0;
      GPIO_InitStruct.Mode 		= mode;
      GPIO_InitStruct.Pull 		= GPIO_NOPULL;
      GPIO_InitStruct.Speed 	= GPIO_SPEED_FREQ_LOW;
      GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
      HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
      break;

    case _DEF_PWM2:
      GPIO_InitStruct.Pin       = GPIO_PIN_6;
      GPIO_InitStruct.Mode		= mode;
      GPIO_InitStruct.Pull 		= GPIO_NOPULL;
      GPIO_InitStruct.Speed 	= GPIO_SPEED_FREQ_LOW;
      GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
      HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
      break;

    case _DEF_PWM3:
      GPIO_InitStruct.Pin       = GPIO_PIN_6;
      GPIO_InitStruct.Mode 		= mode;
      GPIO_InitStruct.Pull 		= GPIO_NOPULL;
      GPIO_InitStruct.Speed 	= GPIO_SPEED_FREQ_LOW;
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

  if(ch >= PWM_MAX_CH) return false;
  if(pulse > pwm_cfg[ch].period) return false;

  p_tim = pwm_tbl[ch].p_tim;

  if(p_tim->Instance == NULL) return false;

  pwm_cfg[ch].pulse = pulse;

  __HAL_TIM_SET_COMPARE(p_tim, pwm_tbl[ch].channel, pulse);

  return true;
}


#ifdef _USE_HW_CLI


static void cliPwm(cli_args_t *args)
{
  bool ret = false;
  uint8_t ch;
  uint32_t value;
  bool cmd_ret;

  if(args->argc == 1)
  {
    if(args->isStr(0, "show") == true)
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
  }

  if(args->argc == 3)
  {
    ch    = (uint8_t)args->getData(1);
    value = (uint32_t)args->getData(2);

    if(args->isStr(0, "run") == true)
    {
      cmd_ret = pwmRunUs(ch, value);

      cliPrintf("pwm run %d %luus : %s\n", ch, value, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "prescaler") == true)
    {
      cmd_ret = pwmSetPrescaler(ch, value);

      cliPrintf("pwm prescaler %d %lu : %s\n", ch, value, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "period") == true)
    {
      cmd_ret = pwmSetPeriod(ch, value);

      cliPrintf("pwm period %d %lu : %s\n", ch, value, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "pulse") == true)
    {
      cmd_ret = pwmSetPulse(ch, value);

      cliPrintf("pwm pulse %d %lu : %s\n", ch, value, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }

    if(args->isStr(0, "duty") == true)
    {
      cmd_ret = false;

      if(ch < PWM_MAX_CH && value <= 100U)
      {
        uint32_t pulse = ((pwm_cfg[ch].period + 1U) * value) / 100U;

        if(pulse > 0)
        {
          pulse--;
        }

        cmd_ret = pwmSetPulse(ch, pulse);
      }

      cliPrintf("pwm duty %d %lu : %s\n", ch, value, cmd_ret ? "OK" : "FAIL");
      ret = true;
    }
  }

  if(ret != true)
  {
    cliPrintf("pwm show\n");
    cliPrintf("pwm run ch[0~%d] time_us\n", PWM_MAX_CH - 1);
    cliPrintf("pwm prescaler ch[0~%d] value\n", PWM_MAX_CH - 1);
    cliPrintf("pwm period ch[0~%d] value\n", PWM_MAX_CH - 1);
    cliPrintf("pwm pulse ch[0~%d] value\n", PWM_MAX_CH - 1);
    cliPrintf("pwm duty ch[0~%d] percent[0~100]\n", PWM_MAX_CH - 1);
  }
}
#endif

#endif
