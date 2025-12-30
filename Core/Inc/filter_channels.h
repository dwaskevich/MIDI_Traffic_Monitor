/*
 * filter_channels.h
 *
 *  Created on: Dec 14, 2025
 *      Author: dwask
 */

#ifndef INC_FILTER_CHANNELS_H_
#define INC_FILTER_CHANNELS_H_

#include <stdint.h>

uint8_t filter_setChannel(uint8_t channel);
uint8_t filter_getChannel(void);
uint8_t filter_resetChannel(void);
void filter_nextChannel(void);
uint8_t filter_setChannelFromEncoder(uint16_t cnt);
char* filter_channelToString(uint8_t channel);

#endif /* INC_FILTER_CHANNELS_H_ */
