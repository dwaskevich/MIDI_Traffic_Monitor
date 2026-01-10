/*
 * midi.h
 *
 *  Created on: Dec 14, 2025
 *      Author: dwask
 */

#ifndef INC_MIDI_H_
#define INC_MIDI_H_

#include <stdbool.h>

/* define data structure for a "frame" of midi data */
typedef struct{
    uint8_t running_status; /* MIDI uses “running status” to omit repeated status bytes */
	uint8_t data[2]; /* data payload (note/velocity/etc) */
    uint32_t time_stamp; /* message arrival time */
    uint8_t channel; /* parsed channel number ... use for filtering display */
} stc_midi;

stc_midi* midi_build_packet(uint8_t rx_byte, uint32_t byte_timestamp);
char *midi_process_message(uint8_t status, uint8_t data1, uint8_t data2);
void midi_handle_sysex(const uint8_t *data, uint8_t length);
void midi_get_note_name(uint8_t note_number, char* buffer);
void midi_setPacketAvailable(void);
void midi_clearPacketAvailable(void);
bool midi_isPacketAvailable(void);


#endif /* INC_MIDI_H_ */
