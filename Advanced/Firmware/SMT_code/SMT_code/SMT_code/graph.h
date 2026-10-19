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
//   top row     : title left ("VOLTAGE" / "CURRENT" / "POWER"), and the mode's
//                 key measurement right-aligned ("RMS: 14.02V", "Peak: 0.405A",
//                 "AVG: 2.625W"), from `headline` in mV / mA / mW
//   middle      : one period of that waveform, auto-scaled to its own peak,
//                 zero line dashed
//   bottom row  : the text in `values` (up to 21 characters)
// Reads the converted data (REAL_MV / REAL_MA), so call it after
// convert_to_real(). Power is v * i at every point.
// Uses about 250 bytes of stack while drawing, no permanent RAM.
void graph_render(graph_mode_t mode, int32_t headline, const char *values);

#endif /* GRAPH_H_ */