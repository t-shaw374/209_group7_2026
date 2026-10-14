/*
 * graph.h
 *
 * Created: 14/10/2026 12:26:41 pm
 *  Author: tsha374
 */ 


#ifndef GRAPH_H_
#define GRAPH_H_

// Draws one full frame of the axis/graph onto the SH1106 display,
// page by page. Uses only a single 128-byte stack buffer internally --
// no persistent RAM cost between calls.
void graph_render(const char *str);



#endif /* GRAPH_H_ */