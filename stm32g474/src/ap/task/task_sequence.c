/*
 * task_sequence.c
 *
 *  Created on: May 29, 2026
 *      Author: young
 */

#include "task/task_sequence.h"

#include "app_sequence.h"

static void threadSequence(void *argument);

bool taskSequenceInit(void)
{
  return osThreadNew(threadSequence, NULL, rtosGetSequenceThreadAttr()) != NULL;
}

static void threadSequence(void *argument)
{
  UNUSED(argument);

  (void)sequenceInit();

  while(1)
  {
    sequenceProcess();
  }
}
