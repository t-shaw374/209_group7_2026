/*
 * oled_graph.c  -  see oled_graph.h
 */

#include "oled.h"
#include "oled_graph.h"
#include "font5x7.h"

#define GRAPH_TOP 9                       /* first graph row */
#define GRAPH_H   (OLED_H - GRAPH_TOP)    /* 23 pixels */

static uint8_t hist[OLED_W];              /* bar height per column, 0..GRAPH_H */
static uint8_t head = 0;                  /* index of the oldest column */

void graph_clear(void)
{
	for (uint8_t k = 0; k < OLED_W; k++) hist[k] = 0;
	head = 0;
}

void graph_push(uint32_t p_mW, uint32_t axis_max_mW)
{
	uint32_t h = axis_max_mW ? (p_mW * GRAPH_H + axis_max_mW / 2) / axis_max_mW : 0;
	if (h > GRAPH_H) h = GRAPH_H;
	if (p_mW > 0 && h == 0) h = 1;        /* any load at all shows at least one pixel */
	hist[head] = (uint8_t)h;
	head = (uint8_t)((head + 1) % OLED_W);
}

void graph_draw(const char *text, uint8_t len)
{
	oled_frame_begin();
	for (uint8_t x = 0; x < OLED_W; x++) {
		uint8_t h = hist[(uint8_t)((head + x) % OLED_W)];
		uint32_t col = font_text_col(text, len, x);               /* rows 0-6 */
		if (h) col |= (0xFFFFFFFFUL << (OLED_H - h));             /* filled bar */
		if (!(x & 3)) col |= 1UL << (OLED_H - 1);                 /* dotted baseline */
		if (h == GRAPH_H) col |= (x & 1) ? (1UL << GRAPH_TOP) : 0; /* clipped: dotted top */
		oled_column(col);
	}
	oled_frame_end();
}
