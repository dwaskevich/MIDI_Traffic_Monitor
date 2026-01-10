/*
 * midi.c
 *
 *  Created on: Dec 14, 2025
 *      Author: dwask
 */

#include "stdint.h"
#include "midi.h"
#include "session.h"
#include "display.h"

/* temporary ... remove after writing midi packet builder (needed for hal tick) */
#include "main.h"
#include "stdio.h"
#include "string.h"

#define MAX_LINES 8
#define MAX_CHARS 22  // 21 visible + null terminator

#define SYSEX_BUFFER_SIZE  16

char temp_buffer[80];
char temp_string[80];

stc_midi midi_packet;
static volatile bool midi_packet_available = false;

volatile uint8_t midi_channel_filter = 0; // 0 = show all, 1–16 = specific channel
uint8_t midi_status = 0;
uint8_t midi_data[2];
uint8_t data_index = 0;
bool in_sysex = false;
uint8_t sysex_buffer[SYSEX_BUFFER_SIZE];
uint8_t sysex_index = 0;

// Lookup table for MIDI note names
const char* note_names[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

stc_midi* midi_build_packet(uint8_t rx_byte, uint32_t byte_timestamp)
{
	if(rx_byte >= 0xF8)
	{
		// Real-time message — handle immediately
		midi_process_message(rx_byte, 0, 0);
		return &midi_packet;
	}

	if(rx_byte == 0xF0)
	{
		midi_packet.time_stamp = byte_timestamp;
	    in_sysex = true;
	    sysex_index = 0;
	}
	else if(rx_byte == 0xF7 && in_sysex)
	{
	    in_sysex = false;
//	    midi_handle_sysex(sysex_buffer, sysex_index);
	}
	else if(in_sysex)
	{
	    if(sysex_index < SYSEX_BUFFER_SIZE)
	        sysex_buffer[sysex_index++] = rx_byte;
	}

	if(rx_byte & 0x80)
    {
        // Status byte
    	midi_packet.time_stamp = byte_timestamp;
        midi_status = rx_byte;
        data_index = 0;

        if(!session_isActive()) /* if this is first byte of new session, set session start time */
		{
			session_start(byte_timestamp);
			session_isActive();
		}

        // Handle 1-byte messages immediately
		if((midi_status & 0xF0) == 0xC0 || (midi_status & 0xF0) == 0xD0)
		{
			// Wait for 1 data byte
			return &midi_packet;
		}

		return &midi_packet;
    }

	// Data byte
	if(midi_status == 0)
		return &midi_packet;; // ignore until we get a valid status

	midi_data[data_index++] = rx_byte;

	uint8_t bytes_needed = ((midi_status & 0xF0) == 0xC0 || (midi_status & 0xF0) == 0xD0) ? 1 : 2;

	if(data_index >= bytes_needed)
	{
		midi_process_message(midi_status, midi_data[0], (bytes_needed == 2) ? midi_data[1] : 0);
		midi_setPacketAvailable();
		data_index = 0;
	}

	return &midi_packet;
}

char *midi_process_message(uint8_t status, uint8_t data1, uint8_t data2)
{
    uint8_t channel = (status & 0x0F) + 1; // MIDI channels are 1–16
    uint8_t type = status & 0xF0;

    midi_packet.channel = channel;
    midi_packet.running_status = status;
    midi_packet.data[0] = data1;
    midi_packet.data[1] = data2;

    char note_str[5];
    static char line[MAX_CHARS];
    midi_get_note_name(data1, note_str);

    if (strlen(note_str) == 3)
        strcat(note_str, " ");

    switch (type) {
        case 0x90:
            if (data2 > 0)
                snprintf(line, MAX_CHARS, "On  Ch%2d %s V%d", channel, note_str, data2);
            else
                snprintf(line, MAX_CHARS, "Off Ch%2d %s", channel, note_str);
            break;
        case 0x80:
            snprintf(line, MAX_CHARS, "Off Ch%2d %s V%d", channel, note_str, data2);
            break;
        case 0xB0:
            snprintf(line, MAX_CHARS, "CC Ch%2d %d=%d", channel, data1, data2);
            break;
        default:
            snprintf(line, MAX_CHARS, "%02X %02X %02X", status, data1, data2);
            break;
    }

    return line;
}

void midi_handle_sysex(const uint8_t *data, uint8_t length)
{
//    if (length < 7) return;  // too short
//    char temp[22];
//
//    if (data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x42 && data[4] == 0x01) {
//        switch (data[5]) {
//            case 0x01: {  // Button action
//                uint8_t bb = data[6];
//                const char *button = NULL;
//                switch (bb) {
//                    case 0x0E: button = "Volume Down"; break;
//                    case 0x0F: button = "Volume Up"; break;
//                    // Add more if desired
//                }
//                if (button) {
//                	snprintf(temp, sizeof(temp), "Miracle: %s", button);
//                	strcpy(midi_packet.display_str, temp);
//                }
//                break;
//            }
//            case 0x03:
//                if (data[6] == 0x01)
//                	strcpy(midi_packet.display_str, "Keyboard overflow");
//                else if (data[6] == 0x02)
//                	strcpy(midi_packet.display_str, "MIDI overflow");
//                break;
//            case 0x05:
//                if (length >= 9) {
//                    char version[8];
//                    snprintf(version, sizeof(version), "%d.%02d", data[6], data[7]);
//                    snprintf(temp, sizeof(temp), "FW Version: %s", version);
//                    strcpy(midi_packet.display_str, temp);
//                }
//                break;
//        }
//    }
}


// Convert MIDI note number (0–127) to string like "C4"
void midi_get_note_name(uint8_t note_number, char* buffer)
{
    uint8_t octave = (note_number / 12) - 1;   // MIDI note 60 = C4
    uint8_t index  = note_number % 12;
    sprintf(buffer, "%s_%d", note_names[index], octave);
}

void midi_setPacketAvailable(void) {
    midi_packet_available = true;
}

void midi_clearPacketAvailable(void) {
    midi_packet_available = false;
}

bool midi_isPacketAvailable(void) {
    return midi_packet_available;
}


