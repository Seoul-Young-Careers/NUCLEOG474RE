/*
 * rtos.c
 *
 *  Created on: May 18, 2026
 *      Author: TEMP
 */

#include "rtos.h"
#include "bsp.h"

#include "FreeRTOS.h"
#include "task.h"

TIM_HandleTypeDef        htim7;

static StaticTask_t mainThread_cb;
static StackType_t  mainThread_stack[_HW_DEF_RTOS_THREAD_MEM_MAIN / sizeof(StackType_t)];

static const osThreadAttr_t mainThread_attributes =
{
  .name       = "mainThread",
  .cb_mem     = &mainThread_cb,
  .cb_size    = sizeof(mainThread_cb),
  .stack_mem  = mainThread_stack,
  .stack_size = sizeof(mainThread_stack),
  .priority   = _HW_DEF_RTOS_THREAD_PRI_MAIN,
};

static StaticTask_t threadLed_cb;
static StackType_t  threadLed_stack[_HW_DEF_RTOS_THREAD_MEM_LED / sizeof(StackType_t)];

static const osThreadAttr_t threadLed_attributes =
{
  .name       = "threadLed",
  .cb_mem     = &threadLed_cb,
  .cb_size    = sizeof(threadLed_cb),
  .stack_mem  = threadLed_stack,
  .stack_size = sizeof(threadLed_stack),
  .priority   = _HW_DEF_RTOS_THREAD_PRI_LED,
};

static StaticTask_t threadMotor_cb;
static StackType_t  threadMotor_stack[_HW_DEF_RTOS_THREAD_MEM_MOTOR / sizeof(StackType_t)];

static const osThreadAttr_t threadMotor_attributes =
{
  .name       = "threadMotor",
  .cb_mem     = &threadMotor_cb,
  .cb_size    = sizeof(threadMotor_cb),
  .stack_mem  = threadMotor_stack,
  .stack_size = sizeof(threadMotor_stack),
  .priority   = _HW_DEF_RTOS_THREAD_PRI_MOTOR,
};

static StaticQueue_t motorMsgQ_cb;
static uint8_t       motorMsgQ_buf[_HW_DEF_RTOS_MSG_Q_MOTOR * sizeof(rtos_motor_msg_t)];

static const osMessageQueueAttr_t motorMsgQ_attributes =
{
  .name    = "motorMsgQ",
  .cb_mem  = &motorMsgQ_cb,
  .cb_size = sizeof(motorMsgQ_cb),
  .mq_mem  = motorMsgQ_buf,
  .mq_size = sizeof(motorMsgQ_buf),
};

const osThreadAttr_t *rtosGetMainThreadAttr(void)
{
  return &mainThread_attributes;
}

const osThreadAttr_t *rtosGetLedThreadAttr(void)
{
  return &threadLed_attributes;
}

const osThreadAttr_t *rtosGetMotorThreadAttr(void)
{
  return &threadMotor_attributes;
}

const osMessageQueueAttr_t *rtosGetMotorMsgQAttr(void)
{
  return &motorMsgQ_attributes;
}

bool rtosInit(void)
{
  return true;
}

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  RCC_ClkInitTypeDef    clkconfig;
  uint32_t              uwTimclock = 0;
  uint32_t              uwPrescalerValue = 0;
  uint32_t              pFLatency;

  HAL_StatusTypeDef     status;

  /* Enable TIM7 clock */
  __HAL_RCC_TIM7_CLK_ENABLE();

  /* Get clock configuration */
  HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

  /* Compute TIM7 clock */
  uwTimclock = HAL_RCC_GetPCLK1Freq();

  /* Compute the prescaler value to have TIM7 counter clock equal to 1MHz */
  uwPrescalerValue = (uint32_t) ((uwTimclock / 1000000U) - 1U);

  /* Initialize TIM7 */
  htim7.Instance = TIM7;

  /* Initialize TIMx peripheral as follow:
   * Period = [(TIM7CLK/1000) - 1]. to have a (1/1000) s time base.
   * Prescaler = (uwTimclock/1000000 - 1) to have a 1MHz counter clock.
   * ClockDivision = 0
   * Counter direction = Up
   */
  htim7.Init.Period = (1000000U / 1000U) - 1U;
  htim7.Init.Prescaler = uwPrescalerValue;
  htim7.Init.ClockDivision = 0;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;

  status = HAL_TIM_Base_Init(&htim7);
  if (status == HAL_OK)
  {

    /* Start the TIM time Base generation in interrupt mode */
    status = HAL_TIM_Base_Start_IT(&htim7);
    if (status == HAL_OK)
    {
    /* Enable the TIM7 global Interrupt */
        HAL_NVIC_EnableIRQ(TIM7_DAC_IRQn);
      /* Configure the SysTick IRQ priority */
      if (TickPriority < (1UL << __NVIC_PRIO_BITS))
      {
        /* Configure the TIM IRQ priority */
        HAL_NVIC_SetPriority(TIM7_DAC_IRQn, TickPriority, 0U);
        uwTickPrio = TickPriority;
      }
      else
      {
        status = HAL_ERROR;
      }
    }
  }

 /* Return function status */
  return status;
}

void HAL_SuspendTick(void)
{
  /* Disable TIM7 update Interrupt */
  __HAL_TIM_DISABLE_IT(&htim7, TIM_IT_UPDATE);
}

/**
  * @brief  Resume Tick increment.
  * @note   Enable the tick increment by Enabling TIM7 update interrupt.
  * @param  None
  * @retval None
  */
void HAL_ResumeTick(void)
{
  /* Enable TIM7 Update interrupt */
  __HAL_TIM_ENABLE_IT(&htim7, TIM_IT_UPDATE);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM7)
  {
    HAL_IncTick();
  }
}
