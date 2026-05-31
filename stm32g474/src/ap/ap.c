/**
 * @file ap.c
 * @author Cho Yongjune (cho0040@naver.com)
 * @brief 
 * @version 0.1
 * @date 2026-05-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "ap.h"

#include "task/task_manager.h"

static void apErrorLoop(void);

void apInit(void)
{
#ifdef _USE_HW_CLI
  cliOpen(_DEF_UART1, 57600);
#endif

  if(taskManagerInit() != true)
  {
    apErrorLoop();
  }
}

void apMain(void)
{
  while(1)
  {
#ifdef _USE_HW_CLI
    cliMain();
#endif
    delay(1);
  }
}

static void apErrorLoop(void)
{
  while(1)
  {
    delay(100);
  }
}
