/*
 * graph.h
 *
 * Created: 1/10/2026 3:43:05 pm
 *  Author: tsha374
 */ 


#ifndef GRAPH_H_
#define GRAPH_H_

// Draws one full frame of the axis/graph onto the SH1106 display,
// page by page. Uses only a single 128-byte stack buffer internally --
// no persistent RAM cost between calls.
void graph_render(void);




#endif /* GRAPH_H_ */