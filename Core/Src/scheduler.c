/*
 * scheduler.c
 *
 *  Created on: Dec 13, 2025
 *      Author: dwask
 */

#include "scheduler.h"

#define MAX_TASKS 8

typedef struct {
    TaskCallback callback;
    uint32_t interval_ms;
    uint32_t last_run;
} Task;

static Task task_list[MAX_TASKS];
static uint8_t task_count = 0;
extern volatile uint32_t ms_counter;

void scheduler_init(void) {
    task_count = 0;
}

void scheduler_add_task(TaskCallback callback, uint32_t interval_ms) {
    if (task_count < MAX_TASKS) {
        task_list[task_count++] = (Task){callback, interval_ms, 0};
    }
}

void run_scheduled_tasks(void) {
    for (uint8_t i = 0; i < task_count; i++) {
        if ((ms_counter - task_list[i].last_run) >= task_list[i].interval_ms) {
            task_list[i].last_run = ms_counter;
            task_list[i].callback();
        }
    }
}



