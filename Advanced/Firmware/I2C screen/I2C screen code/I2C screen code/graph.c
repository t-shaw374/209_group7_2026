/*
 * graph.c
 *
 * Created: 1/10/2026 3:42:56 pm
 *  Author: tsha374
 */ 
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "graph.h"
#include "i2c.h"
#include "sh1106.h"
#include "font_chars.h"

#define CHAR_WIDTH 6   // 5 pixel-columns of glyph + 1 pixel spacing

// ---------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------
#define AXIS_X        16   // column where the vertical (Y) axis is drawn
#define AXIS_Y_ROW    55   // pixel row where the horizontal (X) axis sits
#define AXIS_Y_PAGE   (AXIS_Y_ROW / 8)          // = 6
#define AXIS_Y_BIT    (1 << (AXIS_Y_ROW % 8))   // = 0x80

// ---------------------------------------------------------------------
// Demo data: 22 sample points, y given directly in pixel rows (0..54,
// 0 = top of screen, 54 = just above the x-axis). Stored in flash only.
// Replace this with your own values if driving the graph from real data.
// ---------------------------------------------------------------------
#define NUM_POINTS 22
static const uint8_t demo_data[NUM_POINTS] PROGMEM = {
	50, 45, 38, 30, 22, 15, 10,  8, 10, 15, 22,
	30, 38, 45, 50, 52, 50, 45, 38, 30, 22, 15
};
#define POINT_SPACING 5  // pixels between consecutive points on screen

// ---------------------------------------------------------------------
// Draw a string into a page buffer. Characters whose full 7-row glyph
// doesn't fit within a single page will look clipped -- keep labels on
// pages where that's acceptable (true here for all labels used below).
// ---------------------------------------------------------------------
static void draw_string(uint8_t *buf, uint8_t x, const char *str)
{
	while (*str) {
		const uint8_t *glyph = get_glyph(*str);
		for (uint8_t col = 0; col < 5; col++) {
			if (x + col < SCREEN_WIDTH) {
				buf[x + col] |= pgm_read_byte(&glyph[col]);
			}
		}
		x += CHAR_WIDTH;
		str++;
	}
}

void graph_render(void)
{
	uint8_t buf[SCREEN_WIDTH]; // only RAM this uses: 128 bytes, one page at a time
	
	for (uint8_t page = 0; page < SCREEN_PAGES; page++) {
		
		for (uint8_t i = 0; i < SCREEN_WIDTH; i++) buf[i] = 0x00;
		
		// --- Y axis: a full-height vertical line at column AXIS_X ---
		buf[AXIS_X] = 0xFF;
		
		// --- X axis: a horizontal line, only exists in one page ---
		if (page == AXIS_Y_PAGE) {
			for (uint8_t x = AXIS_X; x < SCREEN_WIDTH; x++) {
				buf[x] |= AXIS_Y_BIT;
			}
		}
		
		// --- Numeric axis labels ---
		if (page == 0) {
			draw_string(buf, 0, "54");   // top-of-scale label
		}
		if (page == AXIS_Y_PAGE) {
			draw_string(buf, 0, "0");    // origin label
		}
		
		// --- Titles ---
		if (page == 0) {
			draw_string(buf, 90, "Y AXIS");  // title near top-right
		}
		if (page == 7) {
			draw_string(buf, 80, "X AXIS");  // title near bottom
		}
		
		// --- Data points: plot whichever samples fall in this page ---
		for (uint8_t i = 0; i < NUM_POINTS; i++) {
			uint8_t y = pgm_read_byte(&demo_data[i]);
			uint8_t point_page = y / 8;
			if (point_page == page) {
				uint8_t x = AXIS_X + 2 + (i * POINT_SPACING);
				if (x < SCREEN_WIDTH) {
					buf[x] |= (1 << (y % 8));
				}
			}
		}
		
		sh1106_set_page(page);
		sh1106_write_page_data(buf, SCREEN_WIDTH);
	}
}
