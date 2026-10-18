/*
 * scratch.h  -  one RAM buffer shared by things that never run at the same time:
 *               the ADC capture buffers (energy monitor) and DOOM's path map.
 */

#ifndef SCRATCH_H_
#define SCRATCH_H_

#include <stdint.h>

#define SCRATCH_WORDS 256                 /* 512 bytes */
extern uint16_t scratch_buf[SCRATCH_WORDS];

#endif /* SCRATCH_H_ */
