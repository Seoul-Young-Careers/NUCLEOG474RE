/*
 * dm542.h
 *
 *  Created on: May 16, 2026
 *      Author: young
 */

#ifndef SRC_COMMON_HW_INCLUDE_STEPMOTOR_DM542_H_
#define SRC_COMMON_HW_INCLUDE_STEPMOTOR_DM542_H_

#include "hw_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _USE_DM542

#define DM542_MAX_CH 		 HW_DM542_MAX
#define DM542_PUL				_DEF_PWM2
#define DM542_DIR				0
#define DM542_LOCK_TIMEOUT_MS  100U

typedef struct
{
  bool is_open;
  bool is_busy;
  int32_t position_step;
  uint32_t remain_step;
} dm542_data_t;

// 비동기 이동이 끝났을 때 상위 task에 알려주기 위한 콜백 타입
typedef void (*dm542_done_callback_t)(uint8_t ch);

bool dm542Init(void);                                                         // DM542 driver init
bool dm542Open(uint8_t ch);                                                   // Open selected DM542 channel

bool dm542IsOpen(uint8_t ch);                                                 // Check channel open state
bool dm542IsBusy(uint8_t ch);                                                 // Check motor output running state

bool dm542Start(uint8_t ch);                                                  // Start step pulse PWM output
bool dm542Stop(uint8_t ch);                                                   // Stop step pulse PWM output

bool dm542SetPrescaler(uint8_t ch, uint32_t prescaler);                       // Apply pulse PWM prescaler
bool dm542SetPeriod(uint8_t ch, uint32_t period);                             // Apply pulse PWM period
bool dm542SetPulse(uint8_t ch, uint32_t pulse);                               // Apply pulse PWM width
bool dm542SetFreq(uint8_t ch, uint32_t freq_hz);                              // Apply step pulse frequency

bool dm542MoveStep(uint8_t ch, int32_t step, uint32_t pulse_delay_us);         // Move by step count
bool dm542MoveStepAsync(uint8_t ch, int32_t step, uint32_t pulse_delay_us);    // step 이동을 시작만 하고 바로 반환
bool dm542AttachDoneCallback(uint8_t ch, dm542_done_callback_t callback);      // 비동기 이동 완료 콜백 등록
bool dm542EnableSensorStop(uint8_t ch, bool enable);                           // 센서 EXTI로 비동기 이동을 멈출 수 있게 설정
bool dm542StopFromISR(uint8_t ch);                                             // 인터럽트 안에서 비동기 이동 정지
bool dm542StopBySensorFromISR(uint8_t ch);                                     // 센서 정지가 허용된 경우에만 ISR에서 정지
bool dm542MoveMm(uint8_t ch, float mm, uint32_t pulse_delay_us);               // Move by distance in mm
bool dm542ReadData(uint8_t ch, dm542_data_t *p_data);                         // Read current driver state

#endif

#ifdef __cplusplus
}
#endif

#endif /* SRC_COMMON_HW_INCLUDE_STEPMOTOR_DM542_H_ */
