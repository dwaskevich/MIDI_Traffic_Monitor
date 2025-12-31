/*
 * ui.c
 *
 *  Created on: Dec 15, 2025
 *      Author: dwask
 */

#include "stdint.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "ui.h"
#include "session.h"
#include "app_state_machine.h"
#include "main.h"
#include "filter_channels.h"
#include "midi.h"

#include "string.h"

/* define number of storage pages for traffic history (limited by available SRAM) */
//#define NUMBER_PAGES    (15u)

/* constants for OLED display */
#define FIRST_DISPLAY_LINE	1u
#define LAST_DISPLAY_LINE	7u
//#define SCROLL_BAR_TOP_PIXEL_POSITION	(DISPLAY_DEFAULT_FONT.height)
#define SCROLL_BAR_TOP_PIXEL_POSITION	(8)
#define SCROLL_BAR_MAX_VERTICAL_SIZE  (SSD1306_HEIGHT - SCROLL_BAR_TOP_PIXEL_POSITION)
#define SCROLL_BAR_MAX_POSITION	(SSD1306_HEIGHT - 7)

/* macro to calculate true mathematical modulo, not c library remainder */
#define MODULO(x, m) ((((x) % (m)) + (m)) % (m))

/* my modulo macro to "wrap around" circular buffer */
#define MYMODULO(x, m) ((x + m) % m)

static uint8_t display_line_pointer = FIRST_DISPLAY_LINE;
static ScrollDirection scroll_direction_indicator = DOWN;

struct CaptureSession {
	bool     is_capture_active;		// Whether a capture session is in progress
	int16_t  midi_total_count;		// Total packet count, running total of midi packets
	bool     has_rollover_occurred;	// Whether rollover of physical memory has occurred
	uint8_t  number_rollovers;		// Number of rollovers that have occurred
    uint16_t number_records;		// Number of records in physical memory (no rollover = total count, rollover = NUMBER_PAGES
    uint16_t newest_index;			// Rollover-protected physical index for newest record
    uint16_t oldest_index;			// Rollover-protected physical index for oldest
};

struct ScrollSession {
	bool     is_scroll_active;		// Whether a scroll session is in progress
	bool	 is_scroll_at_end;		// Whether scroll wheel movement has reached end of history
	int16_t  scroll_index;			// Rollover-protected physical index for scroll record-of-interest
	uint16_t number_records;		// Count of midi packets in history at session entry (new packet arrival while in scroll accounted for in scroll_total_count)
	uint16_t scroll_total_count;	// Local scroll session copy of total midi packet count ... for test if new packets have arrived
	int16_t  display[LAST_DISPLAY_LINE]; // Holds scroll history indexes for painting display during scroll back/forward
	int16_t  new_arrival_total;		// Additional count added to "LIVE" threshold while in scroll session
	int16_t  delta_total;			// Total of all scroll wheel movements during session
    ScrollDirection direction;  	// UI status line up/down arrow display direction indicator
};

static struct CaptureSession capture_session = {0};
static struct ScrollSession scroll_session = {0};

stc_midi_history midi_history[NUMBER_PAGES]; /* declare history array */

extern TIM_HandleTypeDef htim4;
extern volatile bool timeoutFlag;

/* declare and assign pointer to history array (will be used to access history) */
stc_midi_history* ptr_history;

int32_t scroll_bar_movement_ratio = (SCROLL_BAR_MAX_VERTICAL_SIZE * 1024 / NUMBER_PAGES);

uint16_t ui_initialize_ui(void)
{
	ptr_history = midi_history; /* initialize pointer to first element of history array */
	display_line_pointer = FIRST_DISPLAY_LINE;

	ui_draw_scroll_bar(1, SSD1306_HEIGHT, ABSOLUTE, White, false); /* draw an initial scroll bar ... single row of pixels at bottom of scroll bar area */
	ssd1306_FillRectangle(SSD1306_WIDTH - 2, 0, SSD1306_WIDTH, DISPLAY_DEFAULT_FONT.height - 2, Black); /* blank new data indicator */

	capture_session = (struct CaptureSession){0};
	scroll_session = (struct ScrollSession){0};

	/* initialize history */
	for(uint16_t i = 0; i < NUMBER_PAGES; i++)
	{
		ptr_history->packetID = i; /* page identifier (permanent index to this array element) */
		ptr_history++; /* move to next array element */
	}

	ptr_history = midi_history; /* reposition pointer to first element of history array */

	__HAL_TIM_SET_COUNTER(&htim2, 0); /* reset scroll encoder counter */

	return sizeof(midi_history) / sizeof(stc_midi_history); /* confirm total number of elements */
}

void ui_process_midi_packet(stc_midi* ptr_packet)
{
	ui_post_packet_to_history(ptr_packet);
	ui_post_packet_to_display(ptr_packet);
	midi_clearPacketAvailable();
}

void ui_post_packet_to_history(stc_midi* ptr_packet)
{
	/* post to history - put packet in history regardless of APP_STATE or channel filter setting */
	ptr_history->time_stamp = ptr_packet->time_stamp;
	ptr_history->channel = ptr_packet->channel;
	ptr_history->running_status = ptr_packet->running_status;
	ptr_history->data[0] = ptr_packet->data[0];
	ptr_history->data[1] = ptr_packet->data[1];

	ptr_history++; /* move history pointer to next position in circular history buffer */
	/* check for circular rollover */
	if(ptr_history >= &midi_history[NUMBER_PAGES])
	{
		ptr_history = midi_history; /* reset pointer to beginning */
	}

	/* update capture session statistics */
	capture_session.midi_total_count++;
	capture_session.is_capture_active = true;
	if(capture_session.midi_total_count >= NUMBER_PAGES)
	{
		capture_session.has_rollover_occurred = true;
		capture_session.number_rollovers = capture_session.midi_total_count/NUMBER_PAGES;
	}
	capture_session.number_records = capture_session.has_rollover_occurred ? NUMBER_PAGES : capture_session.midi_total_count;
	capture_session.newest_index = MODULO(capture_session.midi_total_count - 1, NUMBER_PAGES);
	if(false == capture_session.has_rollover_occurred)
		capture_session.oldest_index = 0;
	else
		capture_session.oldest_index = MODULO(capture_session.newest_index + 1, NUMBER_PAGES);
}

void ui_post_packet_to_display(stc_midi* ptr_packet)
{
	switch(app_get_state())
	{
		case APP_STATE_MIDI_DISPLAY:
			/* process scroll bar */
			float height = (float)MODULO(capture_session.midi_total_count, NUMBER_PAGES) / NUMBER_PAGES;
			ui_draw_scroll_bar(height, 0, PERCENT, (capture_session.number_rollovers + 1) % 2, capture_session.number_rollovers);

			/* post to display if channel filter matches */
			if(filter_getChannel() == 0 || filter_getChannel() == ptr_packet->channel)
			{
				/* put relative midi session timestamp on status line */
				uint32_t midi_delta_timestamp = session_getDeltaTime(ptr_packet->time_stamp);
				display_status(display_getMode(), midi_delta_timestamp, 0, ui_get_scroll_direction_indicator());

				if(FIRST_DISPLAY_LINE == display_line_pointer) /* display is full, create new blank page */
					display_clear_page(Black);

				/* write most recent history record to current line pointer of display */
				display_string(midi_process_message(ptr_packet->running_status, ptr_packet->data[0], ptr_packet->data[1]), display_line_pointer, 0, White, true);
				display_line_pointer++; /* move display pointer for next arrival */
				if(display_line_pointer > LAST_DISPLAY_LINE)
					display_line_pointer = FIRST_DISPLAY_LINE;
			}
			else /* animate new data arrival indicator to notify ui of non-matching data arrival in background */
			{
				static uint8_t filtered_channel_arrival_count = 0;
				/* indicate new data arrival for other channels */
				ssd1306_FillRectangle(SSD1306_WIDTH - 2, 0, SSD1306_WIDTH, DISPLAY_DEFAULT_FONT.height - 2, Black);
				/* animate new arrival indicator */
				if(filtered_channel_arrival_count++ % 2)
					ssd1306_FillRectangle(SSD1306_WIDTH - 2, 3, SSD1306_WIDTH, DISPLAY_DEFAULT_FONT.height - 2, White);
				else
					ssd1306_FillRectangle(SSD1306_WIDTH - 2, 0, SSD1306_WIDTH, DISPLAY_DEFAULT_FONT.height - 2, White);
				ssd1306_UpdateScreen();
			}
			break;

		case APP_STATE_SCROLL_HISTORY:
			static uint8_t new_arrival_count = 0;
			/* indicate new data arrival in background */
			ssd1306_FillRectangle(SSD1306_WIDTH - 2, 0, SSD1306_WIDTH, DISPLAY_DEFAULT_FONT.height - 2, Black);
			/* animate new arrival indicator */
			if(new_arrival_count++ % 2)
				ssd1306_FillRectangle(SSD1306_WIDTH - 2, 3, SSD1306_WIDTH, DISPLAY_DEFAULT_FONT.height - 2, White);
			else
				ssd1306_FillRectangle(SSD1306_WIDTH - 2, 0, SSD1306_WIDTH, DISPLAY_DEFAULT_FONT.height - 2, White);
			ssd1306_UpdateScreen();
			break;

		default:
			break;
	}
}

int16_t ui_get_filtered_record_index(int16_t index)
{
	/* search backward for next record matching channel filter */
	int16_t filtered_index = index;
	uint16_t number_records_to_check = capture_session.has_rollover_occurred ? capture_session.midi_total_count : NUMBER_PAGES;
	uint16_t i;
	for(i = 0; i < number_records_to_check; i++)
	{
		filtered_index = MYMODULO((index - i), NUMBER_PAGES);
		if(filter_getChannel() == 0 || filter_getChannel() == midi_history[filtered_index].channel)
		{
			return filtered_index; /* matching channel found, return index of matching record */
		}
	}
	return NUMBER_PAGES + 1; /* default return value for "no records found" */
}

void ui_fill_display(void)
{
	int16_t filtered_index;

	for(uint8_t i = 0; i < LAST_DISPLAY_LINE; i++)
	{
		if(scroll_session.display[i] > NUMBER_PAGES)
		{
		  display_string("End of history", i + 1, 0, White, true);
		  break;
		}

		filtered_index = ui_get_filtered_record_index(scroll_session.display[i]);
		if((filtered_index) > NUMBER_PAGES)
		{
		  char temp_buffer[16];
		  sprintf(temp_buffer, "%d No match", scroll_session.display[i]);
		  display_string_to_status_line(temp_buffer, 0);
		  break; /* no records matching channel filter */
		}

		display_string(midi_process_message(midi_history[scroll_session.display[i]].running_status, midi_history[scroll_session.display[i]].data[0], midi_history[scroll_session.display[i]].data[1]), i + 1, 0, White, true);
	}
}

/* abbreviated history during scroll due to oled i2c sluggishness, tim4 timeout will scroll rest of screen history */
void ui_scroll_history(int16_t delta)
{
	char temp_buffer[16];

	if(0 == capture_session.midi_total_count) /* History is empty ... no midi packets received yet, just report message */
	{
		display_string("History is empty", 3, 0, White, true);
		return;
		/* TODO - use TIM4 to replace "History empty" with "Waiting ..." */
	}
	else /* manage scroll wheel input */
	{
		if(false == scroll_session.is_scroll_active) /* first entry to scroll session */
		{
			if(delta > 0) /* --> cw scroll wheel rotation */
			{
				/* do nothing ... can't scroll past current (i.e. newest) index */
				return;
			}
			else /* <-- ccw scroll wheel rotation */
			{
				scroll_session.is_scroll_active = true; /* first entry into scroll session ... change states to active */
				scroll_session.scroll_total_count = capture_session.midi_total_count; /* get capture session total count ... will be used to determine if new data arrived while in scroll */
				scroll_session.number_records = capture_session.number_records; /* set scroll session number of records at entry ... will anchor "end of history" */
				scroll_session.new_arrival_total = 0;
				scroll_session.direction = DOWN;
				ui_set_scroll_direction_indicator(DOWN);
				scroll_session.scroll_index = capture_session.newest_index + 1; /* first entry into scroll session, set scroll index to newest capture session index */
				scroll_session.delta_total = delta; /* scroll wheel delta movements will be used to confine scrolling to newest<>oldest range */
				scroll_session.scroll_index += delta; /* move scroll index to requested record */
				ui_fill_scroll_display_buffer(scroll_session.scroll_index, scroll_session.number_records + scroll_session.delta_total + 1); /* fill display buffer while indexes are known */
			}
		}
		else /* scroll session is already active */
		{
			scroll_session.delta_total += delta;
			if(scroll_session.scroll_total_count != capture_session.midi_total_count)
			{
				/* new data has arrived while in scroll session */
				scroll_session.new_arrival_total = capture_session.midi_total_count - scroll_session.scroll_total_count;
				scroll_session.scroll_total_count = capture_session.midi_total_count; /* update local copy for next test */
			}
			if(delta > 0) /* --> cw scroll wheel rotation */
			{
				scroll_session.is_scroll_at_end = false;
				if(scroll_session.delta_total >= 0 + scroll_session.new_arrival_total)
				{
					/* return to LIVE mode ... can't scroll past current (i.e. newest) index */

					/* kill oneshot timer if app_state back to LIVE */
					HAL_TIM_Base_Stop_IT(&htim4);
					__HAL_TIM_SET_COUNTER(&htim4, 0);
					timeoutFlag = false;

					display_clear_page(Black);
					app_set_state(APP_STATE_MIDI_DISPLAY);
					scroll_session.is_scroll_active = false;
					display_setMode(LIVE);
					ui_restore_display();
					return;
				}
				else /* scroll up from session index */
				{
					scroll_session.direction = UP;
					ui_set_scroll_direction_indicator(UP);
					scroll_session.scroll_index = scroll_session.scroll_index + delta; /* move scroll index to requested record */
					ui_fill_scroll_display_buffer(scroll_session.scroll_index, scroll_session.number_records + scroll_session.delta_total); /* fill display buffer while indexes are known */
				}
			}
			else /* <-- ccw scroll wheel rotation */
			{
				if(scroll_session.delta_total <= 0 - scroll_session.number_records) /* reached end of history ... (capture_session.oldest_index - offset + NUMBER_PAGES) % NUMBER_PAGES) ... no more records to display */
				{
					/* post oldest message but set flag ... can't scroll past oldest */
					scroll_session.delta_total = 1 - scroll_session.number_records;
					scroll_session.scroll_index = capture_session.oldest_index; /* move scroll index to oldest record */
					scroll_session.is_scroll_at_end = true;
					ui_fill_scroll_display_buffer(scroll_session.scroll_index, scroll_session.number_records + scroll_session.delta_total); /* fill display buffer while indexes are known */
				}
				else /* scroll down from session index, adjust scroll index based on delta */
				{
					scroll_session.direction = DOWN;
					ui_set_scroll_direction_indicator(DOWN);
					scroll_session.scroll_index = scroll_session.scroll_index + delta; /* move scroll index to requested record */
					ui_fill_scroll_display_buffer(scroll_session.scroll_index, scroll_session.number_records + scroll_session.delta_total + 1); /* fill display buffer while indexes are known */
				}
			}
		}

		/* finish scroll tasks */
		app_set_state(APP_STATE_SCROLL_HISTORY);

		/* animate scroll bar segment to indicate relative location of current screen within history */
		int16_t scroll_segment_size = scroll_session.number_records < LAST_DISPLAY_LINE ? scroll_session.number_records : LAST_DISPLAY_LINE;
		int16_t scroll_bar_segment_position;
		if(false == capture_session.has_rollover_occurred)
		{
			scroll_bar_movement_ratio = (SCROLL_BAR_MAX_VERTICAL_SIZE * 1024 / scroll_session.number_records);
			scroll_bar_segment_position = SCROLL_BAR_TOP_PIXEL_POSITION + (((scroll_session.number_records - 1) - MYMODULO(scroll_session.scroll_index - capture_session.oldest_index, NUMBER_PAGES)) * scroll_bar_movement_ratio)/1024 + LAST_DISPLAY_LINE;
		}
		else
		{
			scroll_bar_movement_ratio = (SCROLL_BAR_MAX_VERTICAL_SIZE * 1024 / NUMBER_PAGES);
			scroll_bar_segment_position = SCROLL_BAR_TOP_PIXEL_POSITION + (((NUMBER_PAGES - 1) - MYMODULO(scroll_session.scroll_index - capture_session.oldest_index, NUMBER_PAGES)) * scroll_bar_movement_ratio)/1024 + LAST_DISPLAY_LINE;
		}
		if(scroll_session.delta_total < 0) /* clamp scroll bar upward movement if new data has arrived while in scroll */
			ui_draw_scroll_bar(scroll_segment_size - 1, scroll_bar_segment_position, ABSOLUTE, White, false);

		/* build recalled record for display */
		uint32_t midi_delta_timestamp = session_getDeltaTime(midi_history[MYMODULO(scroll_session.scroll_index, NUMBER_PAGES)].time_stamp);
		/* prepare display/screen for requested scroll history */
		display_clear_page(Black);
		/* only display records that match channel filter setting */
		if(filter_getChannel() == 0 || filter_getChannel() == midi_history[MYMODULO(scroll_session.scroll_index, NUMBER_PAGES)].channel)
		{
			display_status(INDEX, midi_delta_timestamp, MYMODULO(scroll_session.scroll_index - capture_session.oldest_index, NUMBER_PAGES), scroll_session.direction);
			display_string(midi_process_message(midi_history[MYMODULO(scroll_session.scroll_index, NUMBER_PAGES)].running_status, midi_history[MYMODULO(scroll_session.scroll_index, NUMBER_PAGES)].data[0], midi_history[MYMODULO(scroll_session.scroll_index, NUMBER_PAGES)].data[1]), 1, 0, White, true);
		}
		else /* no matching records found ... just report message */
		{
			sprintf(temp_buffer, "%d No match", MYMODULO(scroll_session.scroll_index, NUMBER_PAGES));
			display_string_to_status_line(temp_buffer, 0);
		}
		if(true == scroll_session.is_scroll_at_end)
		{
			display_string("End of history", 2, 0, White, true);
		}
	}

	switch (app_get_state())
	{
		case APP_STATE_MIDI_DISPLAY:
			/* kill oneshot timer if app_state back to LIVE */
			HAL_TIM_Base_Stop_IT(&htim4);
			__HAL_TIM_SET_COUNTER(&htim4, 0);
			timeoutFlag = false;
			break;
		case APP_STATE_SCROLL_HISTORY:
			/* restart one-shot timer (timeout period set to .5sec) ... expiration will fill screen with next 6 values */
			HAL_TIM_Base_Stop_IT(&htim4);
			__HAL_TIM_SET_COUNTER(&htim4, 0);
			HAL_TIM_Base_Start_IT(&htim4);
			break;
		default:
			break;
	}
}

int16_t ui_restore_display(void)
{
	int16_t index = ptr_history - midi_history - 1;  /* get index for latest message */
	scroll_session.is_scroll_active = false; /* reset scroll session flag */
	scroll_session.is_scroll_at_end = false;

	/* redraw scroll bar and blank out background data arrival indicator */
	float height = (float)MODULO(capture_session.midi_total_count, NUMBER_PAGES) / NUMBER_PAGES;
	ui_draw_scroll_bar(height, 0, PERCENT, (capture_session.number_rollovers + 1) % 2, capture_session.number_rollovers);
	ssd1306_FillRectangle(SSD1306_WIDTH - 2, 0, SSD1306_WIDTH, DISPLAY_DEFAULT_FONT.height - 2, Black);

	if(filter_getChannel() == 0 || filter_getChannel() == midi_history[index].channel)
	{
		display_clear_page(Black);
		display_line_pointer = FIRST_DISPLAY_LINE;

		/* write most recent history record to first line of display */
		display_string(midi_process_message((ptr_history - 1)->running_status, (ptr_history - 1)->data[0], (ptr_history - 1)->data[1]), FIRST_DISPLAY_LINE, 0, White, true);
		/* put relative midi session timestamp on status line */
		uint32_t midi_delta_timestamp = session_getDeltaTime((ptr_history - 1)->time_stamp);
		display_status(LIVE, midi_delta_timestamp, 0, ui_get_scroll_direction_indicator());
	}
	else
	{
		char temp_buffer[16];
		sprintf(temp_buffer, "%d No match", index);
		display_string_to_status_line(temp_buffer, 0);
	}

	return index;
}

int16_t ui_get_scroll_index(void)
{
	return scroll_session.scroll_index;
}

void ui_set_scroll_direction_indicator(ScrollDirection scroll_direction)
{
	scroll_direction_indicator = scroll_direction;
}

ScrollDirection ui_get_scroll_direction_indicator(void)
{
	return scroll_direction_indicator;
}

void ui_fill_scroll_display_buffer(int16_t index, int16_t number_records)
{
	uint16_t i;
	int16_t temp_index = index;

	for(i = 0; i < number_records; i++)
	{
		if(i >= LAST_DISPLAY_LINE)
			break;
		scroll_session.display[i] = MYMODULO(temp_index--, NUMBER_PAGES);
	}
	for(; i < LAST_DISPLAY_LINE; i++)
	{
		scroll_session.display[i] = NUMBER_PAGES + 1;
	}
}

void ui_jump_to_oldest(void)
{
	scroll_session.is_scroll_active = true; /* make sure scroll state is set to active */
	scroll_session.direction = DOWN;
	ui_set_scroll_direction_indicator(DOWN);
	scroll_session.scroll_index = capture_session.oldest_index; /* set scroll index to oldest capture session index */
	scroll_session.delta_total = 0 - scroll_session.number_records; /* scroll wheel delta movements will be used to confine scrolling to newest<>oldest range */
	ui_fill_scroll_display_buffer(scroll_session.scroll_index, scroll_session.number_records + scroll_session.delta_total + 1); /* fill display buffer while indexes are known */
	display_clear_page(Black);
	display_status(INDEX, 0, MYMODULO(scroll_session.scroll_index - capture_session.oldest_index, NUMBER_PAGES), scroll_session.direction);
	display_string(midi_process_message(midi_history[MYMODULO(scroll_session.scroll_index, NUMBER_PAGES)].running_status, midi_history[MYMODULO(scroll_session.scroll_index, NUMBER_PAGES)].data[0], midi_history[MYMODULO(scroll_session.scroll_index, NUMBER_PAGES)].data[1]), 1, 0, White, true);
	ui_draw_scroll_bar(LAST_DISPLAY_LINE - 1, SSD1306_HEIGHT, ABSOLUTE, White, false);

	/* restart one-shot timer (timeout period set to .5sec) ... expiration will fill screen with next 6 values */
	HAL_TIM_Base_Stop_IT(&htim4);
	__HAL_TIM_SET_COUNTER(&htim4, 0);
	HAL_TIM_Base_Start_IT(&htim4);
}

void ui_draw_scroll_bar(float height, float position, ScrollBarDimensionType dimension_type, SSD1306_COLOR color, bool rollover_indicator)
{
	ssd1306_FillRectangle(SSD1306_WIDTH - 3, SCROLL_BAR_TOP_PIXEL_POSITION, SSD1306_WIDTH, SSD1306_HEIGHT, Black == color ? White : Black);
	switch(dimension_type)
	{
		case PERCENT:
			if(true == rollover_indicator)
				ssd1306_FillRectangle(SSD1306_WIDTH - 3, SCROLL_BAR_TOP_PIXEL_POSITION, SSD1306_WIDTH, SCROLL_BAR_TOP_PIXEL_POSITION + 4, White);
			ssd1306_FillRectangle(SSD1306_WIDTH - 3, SSD1306_HEIGHT - (SCROLL_BAR_MAX_VERTICAL_SIZE * height), SSD1306_WIDTH, SSD1306_HEIGHT, color);
			break;
		case ABSOLUTE:
			if(position > SSD1306_HEIGHT)
				position = SSD1306_HEIGHT;
			ssd1306_FillRectangle(SSD1306_WIDTH - 3, position, SSD1306_WIDTH, position - height, color);
			break;
		default:
			break;
	}

	ssd1306_UpdateScreen();
}

bool ui_is_capture_active(void)
{
    return capture_session.is_capture_active;
}
