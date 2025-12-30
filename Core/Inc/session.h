/*
 * session.h
 *
 *  Created on: Dec 14, 2025
 *      Author: dwask
 */

#ifndef INC_SESSION_H_
#define INC_SESSION_H_

#include <stdbool.h>
#include <stdint.h>

void session_start(uint32_t timestamp_ms);
void session_stop(void);
bool session_isActive(void);
uint32_t session_getStartTimestamp(void);
uint32_t session_getDeltaTime(uint32_t event_timestamp_ms);


#endif /* INC_SESSION_H_ */
