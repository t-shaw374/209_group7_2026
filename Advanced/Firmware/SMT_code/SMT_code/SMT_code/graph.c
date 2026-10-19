/*
 * graph.c
 *
 * Created: 14/10/2026 12:26:31 pm
 *  Author: tsha374
 */
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "graph.h"
#include "i2c.h"
#include "sh1106.h"
#include "font_chars.h"
#include "convert.h"
#include "format.h"

#define CHAR_WIDTH 6   // 5 pixel-columns of glyph + 1 pixel spacing

// ---------------------------------------------------------------------
// Layout (64 rows = 8 pages of 8 rows)
//   page 0      : title left, key measurement right ("RMS: 14.02V")
//   pages 1..6  : graph, rows 8..55
//   page 7      : values line
// ---------------------------------------------------------------------
#define AXIS_X        16                         // column of the vertical axis
#define PLOT_X0       (AXIS_X + 2)               // first graph column
#define PLOT_WIDTH    (SCREEN_WIDTH - PLOT_X0)   // 110 columns
#define PLOT_PAGE_TOP 1
#define PLOT_PAGE_BOT 6
#define ZERO_ROW      31                         // pixel row of the zero line
#define HALF_HEIGHT   22                         // +peak -> row 9, -peak -> row 53

// ---------------------------------------------------------------------
// Draw a string into a page buffer.
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

// One point of the selected waveform, in mV, mA or mW
static int32_t sample(graph_mode_t mode, uint16_t k)
{
	switch (mode) {
		case GRAPH_VOLTAGE: return REAL_MV[k];
		case GRAPH_CURRENT: return REAL_MA[k];
		default:            return ((int32_t)REAL_MV[k] * REAL_MA[k]) / 1000;  // uW -> mW
	}
}

void graph_render(graph_mode_t mode, int32_t headline, const char *values)
{
	static const char t_power[]   PROGMEM = "POWER";
	static const char t_voltage[] PROGMEM = "VOLTAGE";
	static const char t_current[] PROGMEM = "CURRENT";

	static const char l_power[]   PROGMEM = "AVG: ";
	static const char l_voltage[] PROGMEM = "RMS: ";
	static const char l_current[] PROGMEM = "Peak: ";

	const char *title_p;
	const char *label_p;
	char unit;
	switch (mode) {
		case GRAPH_VOLTAGE: title_p = t_voltage; label_p = l_voltage; unit = 'V'; break;
		case GRAPH_CURRENT: title_p = t_current; label_p = l_current; unit = 'A'; break;
		default:            title_p = t_power;   label_p = l_power;   unit = 'W'; break;
	}
	char title[8];
	strcpy_P(title, title_p);

	// ---- Key measurement, right-aligned on the top row, e.g. "RMS: 14.02V" ----
	char hv[16];                                 // "Peak: " (6) + value (7) + '\0'
	char *e = hv;
	strcpy_P(hv, label_p);
	while (*e) e++;                              // end of the label
	e = fmt_milli(e, headline, unit);
	*e = '\0';
	uint8_t hv_x = SCREEN_WIDTH - (uint8_t)(e - hv) * CHAR_WIDTH;

	// ---- Peak, only used to scale the graph to fill its height ----
	int32_t peak = 1;                            // never 0, it's a divisor
	for (uint16_t k = 0; k < ADC_POINTS; k++) {
		int32_t s = sample(mode, k);
		if (s < 0) s = -s;
		if (s > peak) peak = s;
	}

	// ---- Pixel row for every graph column (160 points -> 110 columns) ----
	uint8_t ycol[PLOT_WIDTH];
	for (uint8_t c = 0; c < PLOT_WIDTH; c++) {
		uint16_t k = ((uint16_t)c * ADC_POINTS + PLOT_WIDTH / 2) / PLOT_WIDTH;
		if (k >= ADC_POINTS) k = ADC_POINTS - 1;
		int32_t off = sample(mode, k) * HALF_HEIGHT;
		off = (off >= 0) ? (off + peak / 2) / peak : (off - peak / 2) / peak;
		ycol[c] = (uint8_t)(ZERO_ROW - off);    // |off| <= HALF_HEIGHT
	}

	// ---- Draw, one page at a time ----
	uint8_t buf[SCREEN_WIDTH];

	for (uint8_t page = 0; page < SCREEN_PAGES; page++) {

		for (uint8_t i = 0; i < SCREEN_WIDTH; i++) buf[i] = 0x00;

		if (page == 0) {
			draw_string(buf, 0, title);
			draw_string(buf, hv_x, hv);
		}

		if (page >= PLOT_PAGE_TOP && page <= PLOT_PAGE_BOT) {
			uint8_t top = page * 8;              // first pixel row in this page

			// Vertical axis
			buf[AXIS_X] = 0xFF;

			// Dashed zero line
			if (page == ZERO_ROW / 8) {
				for (uint8_t x = PLOT_X0; x < SCREEN_WIDTH; x += 2) {
					buf[x] |= 1 << (ZERO_ROW % 8);
				}
			}

			// Waveform: each column fills from its own row to the previous
			// column's row, so the points join into a continuous line
			for (uint8_t c = 0; c < PLOT_WIDTH; c++) {
				uint8_t y0 = ycol[c];
				uint8_t y1 = c ? ycol[c - 1] : y0;
				uint8_t lo = (y0 < y1) ? y0 : y1;
				uint8_t hi = (y0 < y1) ? y1 : y0;
				if (lo < top)     lo = top;
				if (hi > top + 7) hi = top + 7;
				for (uint8_t r = lo; r <= hi; r++) {   // skipped if lo > hi (not in this page)
					buf[PLOT_X0 + c] |= 1 << (r - top);
				}
			}
		}

		if (page == 7) {
			draw_string(buf, 0, values);
		}

		sh1106_set_page(page);
		sh1106_write_page_data(buf, SCREEN_WIDTH);
	}
}