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
#include "event_groups.h"

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

static StaticTask_t threadStepMotor_cb;
static StackType_t  threadStepMotor_stack[_HW_DEF_RTOS_THREAD_MEM_STEP_MOTOR / sizeof(StackType_t)];

static const osThreadAttr_t threadStepMotor_attributes =
{
  .name       = "threadStepMotor",
  .cb_mem     = &threadStepMotor_cb,
  .cb_size    = sizeof(threadStepMotor_cb),
  .stack_mem  = threadStepMotor_stack,
  .stack_size = sizeof(threadStepMotor_stack),
  .priority   = _HW_DEF_RTOS_THREAD_PRI_STEP_MOTOR,
};

static StaticQueue_t stepMotorMsgQ_cb;
static uint8_t       stepMotorMsgQ_buf[_HW_DEF_RTOS_MSG_Q_STEP_MOTOR * sizeof(rtos_step_motor_msg_t)];

static const osMessageQueueAttr_t stepMotorMsgQ_attributes =
{
  .name    = "stepMotorMsgQ",
  .cb_mem  = &stepMotorMsgQ_cb,
  .cb_size = sizeof(stepMotorMsgQ_cb),
  .mq_mem  = stepMotorMsgQ_buf,
  .mq_size = sizeof(stepMotorMsgQ_buf),
};

static StaticQueue_t stepMotorAckQ_cb;
static uint8_t       stepMotorAckQ_buf[_HW_DEF_RTOS_MSG_Q_STEP_MOTOR_ACK * sizeof(rtos_step_motor_ack_t)];

static const osMessageQueueAttr_t stepMotorAckQ_attributes =
{
  .name    = "stepMotorAckQ",
  .cb_mem  = &stepMotorAckQ_cb,
  .cb_size = sizeof(stepMotorAckQ_cb),
  .mq_mem  = stepMotorAckQ_buf,
  .mq_size = sizeof(stepMotorAckQ_buf),
};

static StaticTask_t threadButton_cb;
static StackType_t  threadButton_stack[_HW_DEF_RTOS_THREAD_MEM_BUTTON / sizeof(StackType_t)];

static const osThreadAttr_t threadButton_attributes =
{
  .name       = "threadButton",
  .cb_mem     = &threadButton_cb,
  .cb_size    = sizeof(threadButton_cb),
  .stack_mem  = threadButton_stack,
  .stack_size = sizeof(threadButton_stack),
  .priority   = _HW_DEF_RTOS_THREAD_PRI_BUTTON,
};

static StaticTask_t threadSensor_cb;
static StackType_t  threadSensor_stack[_HW_DEF_RTOS_THREAD_MEM_SENSOR / sizeof(StackType_t)];

static const osThreadAttr_t threadSensor_attributes =
{
  .name       = "threadSensor",
  .cb_mem     = &threadSensor_cb,
  .cb_size    = sizeof(threadSensor_cb),
  .stack_mem  = threadSensor_stack,
  .stack_size = sizeof(threadSensor_stack),
  .priority   = _HW_DEF_RTOS_THREAD_PRI_SENSOR,
};

static StaticTask_t threadSequence_cb;
static StackType_t  threadSequence_stack[_HW_DEF_RTOS_THREAD_MEM_SEQUENCE / sizeof(StackType_t)];

static const osThreadAttr_t threadSequence_attributes =
{
  .name       = "threadSequence",
  .cb_mem     = &threadSequence_cb,
  .cb_size    = sizeof(threadSequence_cb),
  .stack_mem  = threadSequence_stack,
  .stack_size = sizeof(threadSequence_stack),
  .priority   = _HW_DEF_RTOS_THREAD_PRI_SEQUENCE,
};

static StaticEventGroup_t appEvent_cb;

static const osEventFlagsAttr_t appEvent_attributes =
{
  .name    = "appEvent",
  .cb_mem  = &appEvent_cb,
  .cb_size = sizeof(appEvent_cb),
};

const osThreadAttr_t *rtosGetMainThreadAttr(void)
{
  return &mainThread_attributes;
}

const osThreadAttr_t *rtosGetLedThreadAttr(void)
{
  return &threadLed_attributes;
}

const osThreadAttr_t *rtosGetStepMotorThreadAttr(void)
{
  return &threadStepMotor_attributes;
}

const osMessageQueueAttr_t *rtosGetStepMotorMsgQAttr(void)
{
  return &stepMotorMsgQ_attributes;
}

const osMessageQueueAttr_t *rtosGetStepMotorAckQAttr(void)
{
  return &stepMotorAckQ_attributes;
}

const osThreadAttr_t *rtosGetButtonThreadAttr(void)
{
  return &threadButton_attributes;
}

const osThreadAttr_t *rtosGetSensorThreadAttr(void)
{
  return &threadSensor_attributes;
}

const osThreadAttr_t *rtosGetSequenceThreadAttr(void)
{
  return &threadSequence_attributes;
}

const osEventFlagsAttr_t *rtosGetAppEventAttr(void)
{
  return &appEvent_attributes;
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
