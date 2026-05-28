/*
 * task_button.c
 *
 *  Created on: May 28, 2026
 *      Author: young
 */

#include "task/task_button.h"
#include "task/app_event.h"

typedef struct
{
  bool stable;
  bool last_raw;
  uint8_t cnt;
} button_db_t;

#define BUTTON_DEBOUNCE_COUNT   3U
#define BUTTON_SCAN_MS          10U

static void threadButton(void *argument);
static bool buttonUpdate(button_db_t *p_btn, bool raw);

bool taskButtonInit(void)
{
  return osThreadNew(threadButton, NULL, rtosGetButtonThreadAttr()) != NULL;
}

static void threadButton(void *argument)
{
  button_db_t reset_btn = {0};
  button_db_t stop_btn  = {0};
  button_db_t start_btn = {0};
  button_db_t foot_btn  = {0};

  UNUSED(argument);

  while(1)
  {
    if(buttonUpdate(&reset_btn, buttonGetPressed(_DEF_BUTTON1)) == true)
    {
      (void)appEventSet(APP_EVT_RESET_REQ);
    }

    if(buttonUpdate(&stop_btn, buttonGetPressed(_DEF_BUTTON2)) == true)
    {
      (void)appEventSet(APP_EVT_STOP_REQ);
    }

    if(buttonUpdate(&start_btn, buttonGetPressed(_DEF_BUTTON3)) == true)
    {
      (void)appEventSet(APP_EVT_START_REQ);
    }

    if(buttonUpdate(&foot_btn, buttonGetPressed(_DEF_BUTTON4)) == true)
    {
      (void)appEventSet(APP_EVT_FOOT_PRESS);
    }

    osDelay(BUTTON_SCAN_MS);
  }
}

static bool buttonUpdate(button_db_t *p_btn, bool raw)
{
  bool pressed_edge = false;

  if(raw != p_btn->last_raw)
  {
    p_btn->last_raw = raw;
    p_btn->cnt = 0U;
  }
  else
  {
    if(p_btn->cnt < BUTTON_DEBOUNCE_COUNT)
    {
      p_btn->cnt++;
    }

    if((p_btn->cnt >= BUTTON_DEBOUNCE_COUNT) && (p_btn->stable != raw))
    {
      p_btn->stable = raw;

      if(raw == true)
      {
        pressed_edge = true;
      }
    }
  }

  return pressed_edge;
}
