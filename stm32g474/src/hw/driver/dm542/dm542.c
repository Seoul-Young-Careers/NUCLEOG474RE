/*
 * dm542.c
 *
 *  Created on: May 16, 2026
 *      Author: young
 */


#include "DM542/dm542.h"
#include "pwm.h"
#include "cli.h"

#ifdef _USE_DM542

#define _PIN_GPIO_DM542_PUL             0
#define _PIN_GPIO_DM542_DIR             1
#define _PIN_GPIO_DM542_ENA             2

typedef struct
{

} dm542_t;

dm542_t dm542_driver;

#endif
