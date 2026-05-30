/*
 * button.c
 *
 *  Created on: Aug 7, 2025
 *      Author: young
 */

#include "button.h"
#include "cli.h"

typedef struct
{
	GPIO_TypeDef  *port;
	uint32_t		pin;

} button_tbl_t;

#define BUTTON_IDLE_PULL       GPIO_PULLUP
#define BUTTON_PRESSED_STATE   GPIO_PIN_RESET

button_tbl_t button_tbl[BUTTON_MAX_CH] =
{
		{GPIOC,GPIO_PIN_5},					//RESET
		{GPIOC,GPIO_PIN_4},					//STOP
		{GPIOA,GPIO_PIN_10},					//START
		{GPIOB,GPIO_PIN_3},					//FOOT SWITCH
};

#ifdef _USE_HW_CLI
static void cliButton(cli_args_t *arg);
#endif

#ifdef _USE_HW_RTOS
static osMutexId_t button_mutex = NULL;
static const osMutexAttr_t button_mutex_attr =
{
  .name      = "button",
  .attr_bits = osMutexRecursive | osMutexPrioInherit,
};
#endif

static bool buttonLock(void);
static void buttonUnlock(void);
static GPIO_PinState buttonReadRaw(uint8_t ch);
static bool buttonIsPressedRaw(uint8_t ch);

bool buttonInit(void)
{
	bool ret = true;

	GPIO_InitTypeDef GPIO_InitStruct = {0};

#ifdef _USE_HW_RTOS
  if(button_mutex == NULL)
  {
    button_mutex = osMutexNew(&button_mutex_attr);
    if(button_mutex == NULL)
    {
      ret = false;
    }
  }
#endif

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();

	/*Configure GPIO pin : PA0 */
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = BUTTON_IDLE_PULL;

	for(int i = 0; i < BUTTON_MAX_CH; i++)
	{
		GPIO_InitStruct.Pin = button_tbl[i].pin;
		HAL_GPIO_Init(button_tbl[i].port, &GPIO_InitStruct);
	}

#ifdef _USE_HW_CLI
	cliAdd("button",cliButton);
#endif
	return ret;
}
bool buttonGetPressed(uint8_t ch)
{
	bool ret = false;

	if(buttonLock() != true) return false;

	if(ch < BUTTON_MAX_CH)
	{
		ret = buttonIsPressedRaw(ch);
	}

	buttonUnlock();

	return ret;
}

bool buttonReadData(uint8_t ch, button_data_t *p_data)
{
	bool ret = false;

	if(buttonLock() != true) return false;

	do
	{
		if(ch >= BUTTON_MAX_CH) break;
		if(p_data == NULL) break;

		p_data->is_pressed = buttonGetPressed(ch);
		ret = true;
	} while(0);

	buttonUnlock();

	return ret;
}

static bool buttonLock(void)
{
#ifdef _USE_HW_RTOS
	if(__get_IPSR() != 0U) return false;

	if((button_mutex != NULL) && (osKernelGetState() == osKernelRunning))
	{
		return osMutexAcquire(button_mutex, BUTTON_LOCK_TIMEOUT_MS) == osOK;
	}
#endif

	return true;
}

static void buttonUnlock(void)
{
#ifdef _USE_HW_RTOS
	if((button_mutex != NULL) && (osKernelGetState() == osKernelRunning))
	{
		(void)osMutexRelease(button_mutex);
	}
#endif
}

static GPIO_PinState buttonReadRaw(uint8_t ch)
{
	return HAL_GPIO_ReadPin(button_tbl[ch].port, button_tbl[ch].pin);
}

static bool buttonIsPressedRaw(uint8_t ch)
{
	return buttonReadRaw(ch) == BUTTON_PRESSED_STATE;
}



#ifdef _USE_HW_CLI
void cliButton(cli_args_t * args)
{
	bool ret = false;

	if(args->argc == 1 && args->isStr(0,"show") == true)
	{
		while(cliKeepLoop())
		{
			for(int i =0; i<BUTTON_MAX_CH; i++)
			{
				button_data_t data;

				if(buttonReadData((uint8_t)i, &data) == true)
				{
					cliPrintf("%d:%d/%d ",
										i,
										buttonReadRaw((uint8_t)i) == GPIO_PIN_SET ? 1 : 0,
										data.is_pressed);
				}
			}
			cliPrintf("\r\n");
			delay(100);
		}
		ret = true;
	}
	if(ret != true)
	{
		cliPrintf("button show\n");
	}
}

#endif
