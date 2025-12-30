/*
 * scheduler.h
 *
 *  Created on: Dec 13, 2025
 *      Author: dwask
 */

#ifndef INC_SCHEDULER_H_
#define INC_SCHEDULER_H_

#include "stdint.h"

#define MAX_TASKS 8

typedef void (*TaskCallback)(void);

void scheduler_init(void);
void scheduler_add_task(TaskCallback callback, uint32_t interval_ms);
void run_scheduled_tasks(void);  // Call this from HAL_SYSTICK_Callback()

#endif /* INC_SCHEDULER_H_ */
