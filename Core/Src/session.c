/*
 * session.c
 *
 *  Created on: Dec 14, 2025
 *      Author: dwask
 */

#include "session.h"

static bool is_midi_session_active = false;
static uint32_t session_start_timestamp = 0;

void session_start(uint32_t timestamp_ms) {
	is_midi_session_active = true;
    session_start_timestamp = timestamp_ms;
}

void session_stop(void) {
	is_midi_session_active = false;
}

bool session_isActive(void) {
    return is_midi_session_active;
}

uint32_t session_getStartTimestamp(void) {
    return session_start_timestamp;
}

uint32_t session_getDeltaTime(uint32_t event_timestamp_ms) {
    if (!is_midi_session_active) {
        return 0;
    }
    return event_timestamp_ms - session_start_timestamp;
}

