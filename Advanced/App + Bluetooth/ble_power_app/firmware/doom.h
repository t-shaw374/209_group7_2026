/*
 * doom.h  -  DOOM mode: runs the game on the OLED until the app sends EXIT
 *
 * Started by the DOOM command. Measurement pauses while you play; the 7-segment
 * display becomes the kill counter. Keys arrive as one-letter lines from the app.
 */

#ifndef DOOM_H_
#define DOOM_H_

void doom_run(void);

#endif /* DOOM_H_ */
