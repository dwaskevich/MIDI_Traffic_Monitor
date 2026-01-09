/*
 * buttons.c
 *
 *  Created on: Dec 13, 2025
 *      Author: dwask
 */

#include "buttons.h"
#include "stm32f1xx_hal.h"
#include "main.h"
#include "stdio.h"
#include "display.h"
#include "filter_channels.h"
#include "session.h"
#include "app_state_machine.h"
#include "ui.h"

#define LONG_PRESS_THRESHOLD_MS 500
#define DEBOUNCE_GUARD_MS 20

typedef struct {
    volatile bool pressed;
    volatile bool released;
    uint32_t press_time;
    uint32_t release_time;
    uint32_t last_event_time;
    ButtonEvent pending_event;
} ButtonState;

static ButtonState button_states[BUTTON_COUNT];

extern TIM_HandleTypeDef htim4;
extern volatile bool timeoutFlag;

void button_init(void) {
    for (int i = 0; i < BUTTON_COUNT; ++i) {
        button_states[i].pressed = false;
        button_states[i].released = false;
        button_states[i].pending_event = BUTTON_EVENT_NONE;
    }
}

void button_exti_trigger(ButtonID id, bool is_pressed) {
    uint32_t now = HAL_GetTick();

    // Debounce: ignore if too soon after last edge
    if ((now - button_states[id].last_event_time) < DEBOUNCE_GUARD_MS)  {
		return;
	}
    button_states[id].last_event_time = now; /* mark current event time */

    if(button_states[id].pressed && is_pressed)
	{
		/* shouldn't happen ... missed release edge/interrupt/event, update press_time to recover */
		button_states[id].press_time = now;
	}
    else if (!button_states[id].pressed && is_pressed) {
		/* button just pressed */
        button_states[id].pressed = true;
        button_states[id].press_time = now;
    } else {
    	// button just released ... always accept release if button was previously pressed
		if(button_states[id].pressed) {
			button_states[id].released = true;
			button_states[id].release_time = now;
		}
    }
}

/* button actions here */
void button_poll(void) {
    for (int i = 0; i < BUTTON_COUNT; ++i) {
    	bool released = false;
		uint32_t press_time = 0, release_time = 0;

		__disable_irq(); // Begin critical section
		if (button_states[i].released) {
			released = true;
			button_states[i].released = false;
			button_states[i].pressed = false;
			press_time = button_states[i].press_time;
			release_time = button_states[i].release_time;
		}
		__enable_irq(); // End critical section

		if (released) {
			uint32_t duration = release_time - press_time;
            if (duration >= LONG_PRESS_THRESHOLD_MS) {
                button_states[i].pending_event = BUTTON_EVENT_LONG_PRESS;
                printf("Button %d: LONG press (%lu ms)\r\n", i, duration);
                switch(i)
                {
                	case BUTTON_SCROLL: /* "zoom" scroll to beginning of session */
                		printf("Recall history from beginning\r\n");
                		ui_jump_to_oldest();

                		break;

                	case BUTTON_FILTER: /* reset history and reset new timestamp to now */
						app_set_state(APP_STATE_MIDI_DISPLAY);
						display_start_screen();
						display_string("New session ...", 1, 0, White, true);
						session_stop();
						uint16_t init_return = ui_initialize_ui();
						__HAL_TIM_SET_COUNTER(&htim2, 0);
						__HAL_TIM_SET_COUNTER(&htim3, 0);
						printf("Reset history, stop current session, history elements initialized = %d\r\n", init_return);

						break;

                	default:
                		break;

                }
            } else {
                button_states[i].pending_event = BUTTON_EVENT_SHORT_PRESS;
                printf("Button %d: SHORT press (%lu ms)\r\n", i, duration);
                switch(i)
				{
					case BUTTON_SCROLL: /* set display mode to LIVE */
						/* kill oneshot timer if app_state back to LIVE */
						HAL_TIM_Base_Stop_IT(&htim4);
						__HAL_TIM_SET_COUNTER(&htim4, 0);
						timeoutFlag = false;
						app_set_state(APP_STATE_MIDI_DISPLAY);
						printf("Set display mode to LIVE\r\n");
						display_setMode(LIVE);
						ui_restore_display();

						break;

					case BUTTON_FILTER: /* reset channel filter to "ALL" */
						printf("Reset channel filter to 'ALL'\r\n");
						__HAL_TIM_SET_COUNTER(&htim3, 0);
						display_channel(filter_resetChannel());

						break;

					default:
						break;

				}
            }
        }
    }
}

ButtonEvent button_get_event(ButtonID id) {
    ButtonEvent event = button_states[id].pending_event;
    button_states[id].pending_event = BUTTON_EVENT_NONE;
    return event;
}
