/*
 * app_state_machine.c
 *
 *  Created on: Dec 13, 2025
 *      Author: dwask
 */

#include "app_state_machine.h"

AppState app_state = APP_STATE_MIDI_DISPLAY;

void app_set_state(AppState new_state) {
    app_state = new_state;
}

AppState app_get_state(void) {
    return app_state;
}
