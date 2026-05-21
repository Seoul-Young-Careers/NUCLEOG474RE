/*
 * hw_def.h
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 *
 *      하드웨어가 공통으로 쓸 관련된 정의들
 */

#ifndef SRC_HW_HW_DEF_H_
#define SRC_HW_HW_DEF_H_

#include "bsp.h"

#define _USE_HW_LED
#define      HW_LED_MAX_CH          1

#define _USE_HW_UART
#define      HW_UART_MAX_CH         1

#define _USE_HW_CLI
#define      HW_CLI_CMD_LIST_MAX    16
#define      HW_CLI_CMD_NAME_MAX    16
#define      HW_CLI_LINE_HIS_MAX    4
#define      HW_CLI_LINE_BUF_MAX    64

#define	_USE_HW_BUTTON
#define		 HW_BUTTON_MAX_CH		1

#define	_USE_HW_GPIO
#define		 HW_GPIO_MAX_CH			1

#define	_USE_HW_SPI
#define 	HW_SPI_MAX_CH			1

//#define _USE_HW_LCD
//#define _USE_HW_ST7735
#define		HW_LCD_WIDTH			160
#define 	HW_LCD_HEIGHT     		160

#define _USE_PWM
#define 	HW_PWM_MAX_CH			3

#define _USE_DM542
#define		HW_DM542_MAX			1

/*
#define _USE_HX711
#define 	HW_HX711_MAX			1
*/

#define _USE_DS3120MG
#define		HW_DS3120MG_MAX		1


#endif /* SRC_HW_HW_DEF_H_ */
