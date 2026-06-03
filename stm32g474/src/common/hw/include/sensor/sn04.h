/*
 * sn04.h
 *
 *  Created on: May 22, 2026
 *      Author: young
 */

#ifndef SRC_COMMON_HW_INCLUDE_SENSOR_SN04_H_
#define SRC_COMMON_HW_INCLUDE_SENSOR_SN04_H_

#include "hw_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _USE_SN04

#define SN04_MAX_CH                    HW_SN04_MAX_CH
#define SN04_LOCK_TIMEOUT_MS           100U

typedef struct
{
  bool is_ready;
  bool is_detected;
} sn04_data_t;

// SN04 감지 상태가 바뀌었을 때 호출되는 콜백 타입
typedef void (*sn04_callback_t)(uint8_t ch, bool detected);

bool sn04Init(void);
bool sn04IsReady(uint8_t ch);

bool sn04Read(uint8_t ch);
bool sn04IsDetected(uint8_t ch);
bool sn04ReadData(uint8_t ch, sn04_data_t *p_data);

bool sn04AttachCallback(sn04_callback_t callback);

#endif

#ifdef __cplusplus
}
#endif

#endif /* SRC_COMMON_HW_INCLUDE_SENSOR_SN04_H_ */
