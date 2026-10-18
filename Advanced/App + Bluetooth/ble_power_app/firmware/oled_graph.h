/*
 * oled_graph.h  -  scrolling power graph on the 128 x 32 OLED (normal mode)
 *
 * Top 8 rows: a line of text built by main.c (e.g. "3.41W 13.0V 0.264A").
 * Rows 9-31:  one column per reading, newest on the right; bar height is
 *             P / axis_max scaled to 23 pixels (computed here, on the ATmega).
 */

#ifndef OLED_GRAPH_H_
#define OLED_GRAPH_H_

#include <stdint.h>

void graph_clear(void);
void graph_push(uint32_t p_mW, uint32_t axis_max_mW);
void graph_draw(const char *text, uint8_t len);

#endif /* OLED_GRAPH_H_ */
