/*
 * app_sequence.h
 *
 *  Created on: May 29, 2026
 *      Author: young
 */

#ifndef SRC_AP_APP_SEQUENCE_H_
#define SRC_AP_APP_SEQUENCE_H_

#include "hw.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  APP_SEQUENCE_STATE_BOOT = 0,       // 전원 인가 후 시퀀스가 아직 초기화되기 전 상태
  APP_SEQUENCE_STATE_HOMING,         // 전원 초기화 또는 RESET으로 원점 복귀 중인 상태
  APP_SEQUENCE_STATE_IDLE_HOME,      // 원점 복귀가 끝나고 HOME 위치에서 대기 중인 상태
  APP_SEQUENCE_STATE_START_ACTION,   // START 시 스텝모터 이동 전에 서보/밸브 동작을 수행 중인 상태
  APP_SEQUENCE_STATE_MOVING_TO_END,  // 스텝모터가 SN04_2 방향으로 이동 중인 상태
  APP_SEQUENCE_STATE_END_ACTION,     // SN04_2 도착 후 서보/밸브 동작을 수행 중인 상태
  APP_SEQUENCE_STATE_READY_SEQUENCE, // START 완료 후 FOOT 스위치 반복 동작을 받을 수 있는 상태
  APP_SEQUENCE_STATE_RUNNING_SEQUENCE, // FOOT 스위치로 반복 시퀀스를 실행 중인 상태
  APP_SEQUENCE_STATE_MOVING_TO_HOME, // STOP 또는 FOOT 반복 중 스텝모터가 SN04_1 방향으로 복귀 중인 상태
  APP_SEQUENCE_STATE_ERROR,          // 명령 실패 또는 센서 미감지 등으로 에러가 발생한 상태
} app_sequence_state_t;

// 장비 시퀀스 초기화 후 원점 복귀를 수행
bool sequenceInit(void);
// 버튼 이벤트를 기다렸다가 해당 시퀀스를 실행
void sequenceProcess(void);
// 현재 시퀀스 상태를 반환
app_sequence_state_t sequenceGetState(void);

#ifdef __cplusplus
}
#endif

#endif /* SRC_AP_APP_SEQUENCE_H_ */
