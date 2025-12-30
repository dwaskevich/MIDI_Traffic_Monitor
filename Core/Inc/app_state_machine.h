/*
 * app_state_machine.h
 *
 *  Created on: Dec 13, 2025
 *      Author: dwask
 */

#ifndef INC_APP_STATE_MACHINE_H_
#define INC_APP_STATE_MACHINE_H_

typedef enum {
    APP_STATE_MIDI_DISPLAY,
    APP_STATE_SCROLL_HISTORY,
	APP_STATE_CONFIG
} AppState;

extern AppState app_state;

void app_set_state(AppState new_state);
AppState app_get_state(void);

#endif /* INC_APP_STATE_MACHINE_H_ */
