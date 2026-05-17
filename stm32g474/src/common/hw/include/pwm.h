/*
 * pwm.h
 *
 *  Created on: May 16, 2026
 *      Author: young
 */

#ifndef SRC_COMMON_HW_INCLUDE_PWM_H_
#define SRC_COMMON_HW_INCLUDE_PWM_H_

#include "hw_def.h"


#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"

#ifdef _USE_PWM

#define PWM_MAX_CH  HW_PWM_MAX_CH

typedef enum
{
  TIM_PWM_ACTIVE_HIGH = 0,
  TIM_PWM_ACTIVE_LOW,
} tim_pwm_active_t;

bool pwmInit(void);                                                    // TIM PWM driver init
bool pwmOpen(uint8_t ch);                                              // Open selected PWM channel

bool pwmIsOpen(uint8_t ch);                                            // Check channel open state
bool pwmIsBusy(uint8_t ch);                                            // Check PWM output running state

bool pwmStart(uint8_t ch);                                             // Start PWM output
bool pwmStop(uint8_t ch);                                              // Stop PWM output

bool pwmSetGpioMode(uint8_t ch, uint32_t mode);                        // Configure PWM GPIO alternate function
bool pwmSetPrescaler(uint8_t ch, uint32_t prescaler);                  // Apply channel prescaler
bool pwmSetPeriod(uint8_t ch, uint32_t period);                        // Apply channel period
bool pwmSetPulse(uint8_t ch, uint32_t pulse);                          // Apply channel pulse width

#endif

#ifdef __cplusplus
}
#endif


#endif /* SRC_COMMON_HW_INCLUDE_PWM_H_ */
