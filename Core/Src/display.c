/*
 * display.c
 *
 *  Created on: Dec 13, 2025
 *      Author: dwask
 */

#include "stdio.h"
#include "string.h"
#include "display.h"
#include "filter_channels.h"
#include "ui.h"


#define STATUS_LINE_LINE_NUMBER  0

char hello_world_str[] = "MIDI Traffic Monitor";
char print_buffer[128];
uint8_t line_number = 0;

static uint8_t line_height;
static uint8_t chars_per_line;
static uint8_t number_lines;
static uint8_t display_mode = LIVE;

void display_init(void)
{
  ssd1306_Init();
  line_height = DISPLAY_DEFAULT_FONT.height + 1;
  chars_per_line = SSD1306_WIDTH / DISPLAY_DEFAULT_FONT.width;
  number_lines = (SSD1306_HEIGHT / DISPLAY_DEFAULT_FONT.height);
}

void display_splash_screen()
{
  ssd1306_FillRectangle(0, 0, SSD1306_WIDTH, 14, White);
  ssd1306_FillRectangle(2, 3, 125, 11, Black);
  ssd1306_SetCursor(4, 4);
  ssd1306_WriteString(hello_world_str, DISPLAY_DEFAULT_FONT, White);
  line_number = DISPLAY_DEFAULT_FONT.height * 2;
  ssd1306_SetCursor(3, 20);
  ssd1306_WriteString("SSD1306 OLED 128x64", DISPLAY_DEFAULT_FONT, White);
  sprintf(print_buffer, "Font -> H=%d, W=%d", DISPLAY_DEFAULT_FONT.height, DISPLAY_DEFAULT_FONT.width);
  line_number = DISPLAY_DEFAULT_FONT.height * 3 + 1;
  ssd1306_SetCursor(3, 29);
  ssd1306_WriteString(print_buffer, DISPLAY_DEFAULT_FONT, White);
  sprintf(print_buffer, "Characters/line = %d", chars_per_line);
  line_number = DISPLAY_DEFAULT_FONT.height * 4 + 2;
  ssd1306_SetCursor(3, 38);
  ssd1306_WriteString(print_buffer, DISPLAY_DEFAULT_FONT, White);
  sprintf(print_buffer, "Number of lines = %d", number_lines);
  line_number = DISPLAY_DEFAULT_FONT.height * 5 + 3;
  ssd1306_SetCursor(3, 47);
  ssd1306_WriteString(print_buffer, DISPLAY_DEFAULT_FONT, White);
  sprintf(print_buffer, "MIDI history = %d", NUMBER_PAGES );
  line_number = DISPLAY_DEFAULT_FONT.height * 6 + 4;
  ssd1306_SetCursor(3, 56);
  ssd1306_WriteString(print_buffer, DISPLAY_DEFAULT_FONT, White);

  ssd1306_UpdateScreen();
}

void display_clear_screen(SSD1306_COLOR color)
{
	ssd1306_Fill(color);
	ssd1306_UpdateScreen();
}

void display_start_screen(void)
{
  display_clear_screen(Black);
  display_status(LIVE, 0, 0, DOWN);
  ssd1306_Line((STATUS_LINE_STATUS_WIDTH + 1) * DISPLAY_DEFAULT_FONT.width, 0, (STATUS_LINE_STATUS_WIDTH + 1) * DISPLAY_DEFAULT_FONT.width, DISPLAY_DEFAULT_FONT.height - 2, White);
  display_channel(filter_getChannel());
  display_string("Waiting ...", 1, 0, White, true);
}

uint8_t display_setMode(StatusDisplayModes mode)
{
	display_mode = mode;
	return display_mode;
}

uint8_t display_getMode(void)
{
	return display_mode;
}

int16_t display_string(char *str, uint8_t line_number, uint8_t cursor_position, SSD1306_COLOR color, bool ceol_flag)
{
	uint8_t x_position = cursor_position * DISPLAY_DEFAULT_FONT.width;
	uint8_t cursor_end_position = cursor_position + strlen(str);
	if(str == NULL || cursor_end_position > chars_per_line)
		return -1;

	if(line_number > number_lines - 1)
		line_number = number_lines - 1;
	ssd1306_SetCursor(x_position, line_number * line_height);
	ssd1306_WriteString(str, DISPLAY_DEFAULT_FONT, color);
	if(true == ceol_flag)
	{
//		while(cursor_end_position++ < chars_per_line)
		while(cursor_end_position++ < chars_per_line - 1)
			ssd1306_WriteChar(' ', DISPLAY_DEFAULT_FONT, color);
	}
	ssd1306_UpdateScreen();
	return 0;
}

int16_t display_status(StatusDisplayModes mode, uint32_t time_stamp, uint16_t index, ScrollDirection arrow_direction)
{
	if(time_stamp > 99999999)
		time_stamp = 99999999;
	if(LIVE == mode)
	{
		sprintf(print_buffer, "LIVE %lu", (unsigned long)time_stamp);
		display_string(print_buffer, STATUS_LINE_LINE_NUMBER, 0, White, false);
		uint8_t cursor_end_position = strlen(print_buffer);
		while(cursor_end_position++ < STATUS_LINE_STATUS_WIDTH)
			ssd1306_WriteChar(' ', DISPLAY_DEFAULT_FONT, White);
	}
	else if(SCROLL == mode)
	{
		sprintf(print_buffer, "SCRL %lu", (unsigned long)time_stamp);
		display_string(print_buffer, STATUS_LINE_LINE_NUMBER, 0, White, false);
		uint8_t cursor_end_position = strlen(print_buffer);
		while(cursor_end_position++ < STATUS_LINE_STATUS_WIDTH)
			ssd1306_WriteChar(' ', DISPLAY_DEFAULT_FONT, White);
	}
	else if(INDEX == mode)
	{
		display_draw_scroll_arrow(ui_get_scroll_direction_indicator());
		sprintf(print_buffer, "%d %lu", index, (unsigned long)time_stamp);
		display_string(print_buffer, STATUS_LINE_LINE_NUMBER, 1, White, false);
		uint8_t cursor_end_position = strlen(print_buffer);
		while(cursor_end_position++ < STATUS_LINE_STATUS_WIDTH)
			ssd1306_WriteChar(' ', DISPLAY_DEFAULT_FONT, White);
	}

	ssd1306_UpdateScreen();

	return 0;
}
int16_t display_string_to_status_line(char *str, uint8_t position)
{
	display_string(str, STATUS_LINE_LINE_NUMBER, position, White, false);
	uint8_t cursor_end_position = strlen(str);
	while(cursor_end_position++ < STATUS_LINE_STATUS_WIDTH)
		ssd1306_WriteChar(' ', DISPLAY_DEFAULT_FONT, White);

	return 0;
}

int16_t display_channel(uint8_t channel)
{
	display_string(filter_channelToString(channel), STATUS_LINE_LINE_NUMBER, STATUS_LINE_STATUS_WIDTH + 2, White, true);

	return channel;
}

void display_clear_page(SSD1306_COLOR color)
{
//	ssd1306_FillRectangle(0, line_height, SSD1306_WIDTH, SSD1306_HEIGHT, Black);
	ssd1306_FillRectangle(0, line_height, SSD1306_WIDTH - 4, SSD1306_HEIGHT, Black);
}

void display_draw_scroll_arrow(ScrollDirection arrow_direction)
{
	ssd1306_SetCursor(0, 0);
	ssd1306_WriteChar('|', DISPLAY_DEFAULT_FONT, White);

	if(UP == arrow_direction)
	{
		ssd1306_DrawPixel(0, 2, White);
		ssd1306_DrawPixel(1, 1, White);
		ssd1306_DrawPixel(3, 1, White);
		ssd1306_DrawPixel(4, 2, White);
	}
	else
	{
		ssd1306_DrawPixel(0, 4, White);
		ssd1306_DrawPixel(1, 5, White);
		ssd1306_DrawPixel(3, 5, White);
		ssd1306_DrawPixel(4, 4, White);
	}
}
