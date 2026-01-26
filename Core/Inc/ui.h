/*
 * ui.h
 *
 *  Created on: Dec 15, 2025
 *      Author: dwask
 */

#ifndef INC_UI_H_
#define INC_UI_H_

#include "midi.h"
#include "display.h"

/* define data structure for midi data history */
typedef struct{
	uint16_t packetID; /* permanent index */
    uint8_t running_status; /* MIDI uses “running status” to omit repeated status bytes */
	uint8_t data[2]; /* data payload (note/velocity/etc) */
    uint32_t time_stamp; /* message arrival time ... timestamp of first byte passed from midi_build_packet() */
    uint8_t channel; /* parsed channel number ... use for filtering display */
} stc_midi_history;

typedef enum {
	PERCENT,
	ABSOLUTE
} ScrollBarDimensionType;

/* define number of storage pages for traffic history (limited by available SRAM) */
#define NUMBER_PAGES    (700u)

extern uint16_t midi_total_count;

typedef struct ScrollSession ScrollSession;
const ScrollSession* scroll_session_get(void);

uint16_t ui_initialize_ui(void);
void ui_post_packet_to_display(stc_midi* ptr_midi_packet);
void ui_process_midi_packet(stc_midi* ptr_packet);
void ui_post_packet_to_history(stc_midi* ptr_packet);
void ui_fill_display(void);
void ui_scroll_history(int16_t delta);
int16_t ui_get_filtered_record_index(int16_t index, uint16_t number_records_to_check, ScrollDirection direction);
int16_t ui_restore_display(void);
void ui_fill_scroll_display_buffer(int16_t index, int16_t number_records);
void ui_set_scroll_direction_indicator(ScrollDirection scroll_direction);
ScrollDirection ui_get_scroll_direction_indicator(void);
void ui_jump_to_oldest(void);
void ui_draw_scroll_bar(float height, float position, ScrollBarDimensionType dimension_type, SSD1306_COLOR color, bool rollover_indicator);
bool ui_is_capture_active(void);

#endif /* INC_UI_H_ */
