/*
 * task_sensor.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/task_sensor.h"
#include "task/app_event.h"

#define SENSOR_SCAN_MS          10U

static void threadSensor(void *argument);
static uint32_t sensorGetSn04EventBit(uint8_t ch);

#ifdef _USE_SN04
static void sensorSn04IsrHandler(uint8_t ch, bool detected);
#endif

bool taskSensorInit(void)
{
#ifdef _USE_SN04
  (void)sn04SetIsrCallback(sensorSn04IsrHandler);
#endif

  return osThreadNew(threadSensor, NULL, rtosGetSensorThreadAttr()) != NULL;
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
    (void)appEventSet(evt_bit);
  }
  else
  {
    (void)appEventClear(evt_bit);
  }
}
#endif
