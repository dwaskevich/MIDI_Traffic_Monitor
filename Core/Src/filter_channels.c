/*
 * filter_channels.c
 *
 *  Created on: Dec 14, 2025
 *      Author: dwask
 */

#include "stdio.h"
#include "filter_channels.h"

static uint8_t filter_channel = 0;

uint8_t filter_setChannel(uint8_t channel) {
    filter_channel = channel % 17;  // 0–16, where 0 = ALL
    return filter_channel;
}

uint8_t filter_getChannel(void) {
    return filter_channel;
}

uint8_t filter_resetChannel(void) {
    filter_channel = 0;
    return filter_channel;
}

void filter_nextChannel(void) {
    filter_channel = (filter_channel + 1) % 17;
}

uint8_t filter_setChannelFromEncoder(uint16_t cnt) {
    // Assuming 4 counts per detent and 17 positions (0 = ALL, 1–16 = channels)
    uint8_t channel = (cnt / 4) % 17;
    filter_setChannel(channel);
    return channel;
}

// Note - this function uses a static buffer, so returned pointer only valid until next call.
char* filter_channelToString(uint8_t channel) {
    static char buffer[8];
    if (channel == 0) {
        return "Ch ALL";
    } else {
        snprintf(buffer, sizeof(buffer), "Ch %-3d", channel);
        return buffer;
    }
}



