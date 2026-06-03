/*
 * task_sensor.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/task_sensor.h"
#include "task/app_event.h"

#define SENSOR_SCAN_MS          10

// 이미 눌린 limit 센서에서 반대 방향으로 빠져나갈 때, 해당 센서만 DM542 즉시 정지에서 제외한다.
static volatile uint32_t sensor_dm542_stop_ignore_evt = 0U;

static void threadSensor(void *argument);
static uint32_t sensorGetSn04EventBit(uint8_t ch);

#ifdef _USE_SN04
static void sensorSn04IsrHandler(uint8_t ch, bool detected);
#endif

bool taskSensorInit(void)
{
#ifdef _USE_SN04
  (void)sn04AttachCallback(sensorSn04IsrHandler);
#endif

  return osThreadNew(threadSensor, NULL, rtosGetSensorThreadAttr()) != NULL;
}

void taskSensorSetDm542StopIgnore(uint32_t evt_mask)
{
  sensor_dm542_stop_ignore_evt = evt_mask;
}

void taskSensorClearDm542StopIgnore(uint32_t evt_mask)
{
  sensor_dm542_stop_ignore_evt &= ~evt_mask;
}

static void threadSensor(void *argument)
{
  UNUSED(argument);

  while(1)
  {
#ifdef _USE_SN04
    for(uint8_t i = 0; i < SN04_MAX_CH; i++)
    {
      uint32_t evt_bit;

      evt_bit = sensorGetSn04EventBit(i);
      if(evt_bit == 0U)
      {
        continue;
      }

      if(sn04IsDetected(i) == true)
      {
        (void)appEventSet(evt_bit);
      }
      else
      {
        taskSensorClearDm542StopIgnore(evt_bit);
        (void)appEventClear(evt_bit);
      }
    }
#endif

    osDelay(SENSOR_SCAN_MS);
  }
}

static uint32_t sensorGetSn04EventBit(uint8_t ch)
{
  switch(ch)
  {
    case _DEF_SN04_1:
      return APP_EVT_SN04_1_DETECTED;

    case _DEF_SN04_2:
      return APP_EVT_SN04_2_DETECTED;

    default:
      return 0U;
  }
}

#ifdef _USE_SN04
static void sensorSn04IsrHandler(uint8_t ch, bool detected)
{
  uint32_t evt_bit;

  evt_bit = sensorGetSn04EventBit(ch);
  if(evt_bit == 0U) return;

  if(detected == true)
  {
    // 빠져나가는 중인 limit 센서는 release 전까지 DM542 ISR 정지에서 제외한다.
    if((sensor_dm542_stop_ignore_evt & evt_bit) == 0U)
    {
      (void)dm542StopBySensorFromISR(_DEF_DM542_1);
    }

    (void)appEventSet(evt_bit);
  }
  else
  {
    taskSensorClearDm542StopIgnore(evt_bit);
    (void)appEventClear(evt_bit);
  }
}
#endif
