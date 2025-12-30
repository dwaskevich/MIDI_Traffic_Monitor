/*
 * display.h
 *
 *  Created on: Dec 13, 2025
 *      Author: dwask
 */

#ifndef INC_DISPLAY_H_
#define INC_DISPLAY_H_

#include "stdbool.h"
#include "ssd1306.h"
#include "ssd1306_conf.h"
#include "ssd1306_fonts.h"

#define DISPLAY_DEFAULT_FONT Font_6x8
#define STATUS_LINE_STATUS_WIDTH  13

typedef enum {
    STATUS_FIELD = 0,
    FILTER_FIELD,
    MIDI_LINE
} DisplayFeature;

typedef enum {
    LIVE = 0,
    SCROLL,
	INDEX
} StatusDisplayModes;

typedef enum {
    UP = 0,
    DOWN
} ScrollDirection;

void display_init(void);
void display_splash_screen(void);
void display_clear_screen(SSD1306_COLOR color);
void display_start_screen(void);
uint8_t display_setMode(StatusDisplayModes mode);
uint8_t display_getMode(void);
int16_t display_string(char *str, uint8_t line_number, uint8_t cursor_position, SSD1306_COLOR color, bool ceol_flag);
int16_t display_status(StatusDisplayModes mode, uint32_t time_stamp, uint16_t index, ScrollDirection arrow_direction);
int16_t display_string_to_status_line(char *str, uint8_t position);
int16_t display_channel(uint8_t channel);
void display_clear_page(SSD1306_COLOR color);
void display_draw_scroll_arrow(ScrollDirection arrow_direction);

#endif /* INC_DISPLAY_H_ */
