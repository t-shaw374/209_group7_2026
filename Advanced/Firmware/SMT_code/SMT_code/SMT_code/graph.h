/*
 * graph.h
 *
 * Created: 14/10/2026 12:26:41 pm
 *  Author: tsha374
 */


#ifndef GRAPH_H_
#define GRAPH_H_

#include <stdint.h>

typedef enum {
	GRAPH_POWER = 0,
	GRAPH_VOLTAGE,
	GRAPH_CURRENT,
	GRAPH_MODES               // number of modes, for cycling through them
} graph_mode_t;

// Draws one full frame onto the SH1106 display, page by page:
//   top row     : title ("POWER" / "VOLTAGE" / "CURRENT") and the peak value,
//                 which is the height of the graph's top and bottom edge
//   middle      : one period of that waveform, auto-scaled, zero line dashed
//   bottom row  : the text in `values` (up to 21 characters)
// Reads the converted data (REAL_MV / REAL_MA), so call it after
// convert_to_real(). Power is v * i at every point.
// Uses about 250 bytes of stack while drawing, no permanent RAM.
void graph_render(graph_mode_t mode, const char *values);

#endif /* GRAPH_H_ */