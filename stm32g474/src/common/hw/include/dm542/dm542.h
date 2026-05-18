/*
 * dm542.h
 *
 *  Created on: May 16, 2026
 *      Author: young
 */

#ifndef SRC_COMMON_HW_INCLUDE_DM542_DM542_H_
#define SRC_COMMON_HW_INCLUDE_DM542_DM542_H_

#include "hw_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _USE_DM542

#ifdef HW_DM542_MAX_CH
#define DM542_MAX_CH  HW_DM542_MAX_CH
#endif

typedef enum
{
  DM542_DIR_CW = 0,
  DM542_DIR_CCW,
} dm542_dir_t;

bool dm542Init(void);                                                         // DM542 driver init
bool dm542Open(uint8_t ch);                                                   // Open selected DM542 channel

bool dm542IsOpen(uint8_t ch);                                                 // Check channel open state
bool dm542IsBusy(uint8_t ch);                                                 // Check motor output running state
bool dm542IsEnabled(uint8_t ch);                                              // Check driver enable state

bool dm542Enable(uint8_t ch);                                                 // Enable motor driver output
bool dm542Disable(uint8_t ch);                                                // Disable motor driver output

bool dm542SetDir(uint8_t ch, dm542_dir_t dir);                                // Set motor rotation direction
bool dm542GetDir(uint8_t ch, dm542_dir_t *p_dir);                             // Get motor rotation direction

bool dm542Start(uint8_t ch);                                                  // Start step pulse PWM output
bool dm542Stop(uint8_t ch);                                                   // Stop step pulse PWM output
bool dm542Step(uint8_t ch);                                                   // Generate one step pulse

bool dm542SetPrescaler(uint8_t ch, uint32_t prescaler);                       // Apply pulse PWM prescaler
bool dm542SetPeriod(uint8_t ch, uint32_t period);                             // Apply pulse PWM period
bool dm542SetPulse(uint8_t ch, uint32_t pulse);                               // Apply pulse PWM width
bool dm542SetFreq(uint8_t ch, uint32_t freq_hz);                              // Apply step pulse frequency

bool dm542MoveStep(uint8_t ch, int32_t step, uint32_t pulse_delay_us);         // Move by step count
bool dm542MoveMm(uint8_t ch, float mm, uint32_t pulse_delay_us);               // Move by distance in mm

#endif

#ifdef __cplusplus
}
#endif

#endif /* SRC_COMMON_HW_INCLUDE_DM542_DM542_H_ */
