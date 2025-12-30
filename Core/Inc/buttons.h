/*
 * buttons.h
 *
 *  Created on: Dec 13, 2025
 *      Author: dwask
 */

#ifndef INC_BUTTONS_H_
#define INC_BUTTONS_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BUTTON_SCROLL = 0,
    BUTTON_FILTER,
    BUTTON_COUNT
} ButtonID;

typedef enum {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_SHORT_PRESS,
    BUTTON_EVENT_LONG_PRESS
} ButtonEvent;

void button_init(void);
void button_exti_trigger(ButtonID id);  // Call from EXTI ISR
void button_poll(void);                 // Call from scheduled task

ButtonEvent button_get_event(ButtonID id);  // Poll for events

#endif /* INC_BUTTONS_H_ */
