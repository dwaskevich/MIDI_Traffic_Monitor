/*
 * tasks.c
 *
 *  Created on: Dec 13, 2025
 *      Author: dwask
 */

#include "stdio.h"
#include "stdlib.h"
#include "scheduler.h"
#include "tasks.h"
#include "main.h"  // For GPIO or other shared symbols
#include "buttons.h"
#include "app_state_machine.h"
#include "display.h"
#include "filter_channels.h"
#include "ui.h"

extern volatile bool timeoutFlag;

// Task implementation

void heartbeat(void) {
	HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}

void read_encoders(void)
{
	static int16_t rotary_scroll_previous_value = 0, rotary_filter_previous_value = 0;

	int16_t rotary_scroll_current_value = (int16_t)__HAL_TIM_GET_COUNTER(&htim2) / 2;
	int16_t rotary_filter_current_value = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);

	int16_t delta = (int16_t)(rotary_scroll_current_value - rotary_scroll_previous_value);
	rotary_scroll_previous_value = rotary_scroll_current_value;
	if (0 != delta)
	{
		ui_scroll_history(delta); /* scroll wheel active, send delta to ui for processing */
	}

	if(rotary_filter_current_value != rotary_filter_previous_value)
	{
		printf("Filter raw = %d\r\n", rotary_filter_current_value);

		display_channel(filter_setChannelFromEncoder(rotary_filter_current_value));
		rotary_filter_previous_value = rotary_filter_current_value;
	}
}

static void poll_buttons(void) {
    button_poll();

    switch (button_get_event(BUTTON_SCROLL)) {
        case BUTTON_EVENT_SHORT_PRESS:
            app_set_state(APP_STATE_MIDI_DISPLAY);
            break;
        case BUTTON_EVENT_LONG_PRESS:
            app_set_state(APP_STATE_MIDI_DISPLAY);
            break;
        default:
            break;
    }

    switch (button_get_event(BUTTON_FILTER)) {
        case BUTTON_EVENT_SHORT_PRESS:
            // e.g., scroll up
            break;
        case BUTTON_EVENT_LONG_PRESS:
            // e.g., reset filter
            break;
        default:
            break;
    }
}

// --- Task initialization/registration ---
void tasks_init(void) {
    scheduler_add_task(heartbeat, 500);
    scheduler_add_task(read_encoders, 21);
    scheduler_add_task(poll_buttons, 13);
    // Add more tasks here
}


