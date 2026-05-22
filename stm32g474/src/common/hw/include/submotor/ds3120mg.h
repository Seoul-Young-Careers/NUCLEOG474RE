/*
 * ds3120mg.h
 *
 *  Created on: May 20, 2026
 *      Author: young
 */

#ifndef SRC_COMMON_HW_INCLUDE_SUBMOTOR_DS3120MG_H_
#define SRC_COMMON_HW_INCLUDE_SUBMOTOR_DS3120MG_H_

#include "hw_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _USE_DS3120MG

#define DS3120MG_MAX_CH                 HW_DS3120MG_MAX

#define DS3120MG_DEFAULT_FREQ_HZ        300U

#define DS3120MG_MIN_PULSE_US           500U
#define DS3120MG_MID_PULSE_US           1500U
#define DS3120MG_MAX_PULSE_US           2500U
#define DS3120MG_DEFAULT_ANGLE_DEG      90.0f
#define DS3120MG_MAX_ANGLE_DEG          180.0f

typedef struct
{
  uint16_t freq_hz;
  uint16_t min_pulse_us;
  uint16_t mid_pulse_us;
  uint16_t max_pulse_us;
  uint16_t pulse_us;
  float angle_deg;
  float max_angle_deg;
} ds3120mg_data_t;

bool ds3120mgInit(void);                                                     // DS3120MG driver init
bool ds3120mgOpen(uint8_t ch);                                               // Open selected servo channel

bool ds3120mgIsOpen(uint8_t ch);                                             // Check channel open state
bool ds3120mgIsStarted(uint8_t ch);                                          // Check PWM output state

bool ds3120mgStart(uint8_t ch);                                              // Start servo PWM output
bool ds3120mgStop(uint8_t ch);                                               // Stop servo PWM output

bool ds3120mgSetPulseUs(uint8_t ch, uint16_t pulse_us);                      // Set servo pulse width
bool ds3120mgSetAngle(uint8_t ch, float angle_deg);                          // Set servo angle
bool ds3120mgCenter(uint8_t ch);                                             // Move servo to neutral position

uint16_t ds3120mgGetPulseUs(uint8_t ch);                                     // Get current pulse width
float ds3120mgGetAngle(uint8_t ch);                                          // Get current angle

bool ds3120mgReadData(uint8_t ch, ds3120mg_data_t *p_data);                  // Read current servo settings

#endif

#ifdef __cplusplus
}
#endif

#endif /* SRC_COMMON_HW_INCLUDE_SUBMOTOR_DS3120MG_H_ */
