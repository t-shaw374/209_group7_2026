/*
 * monitor.h  -  shared state of the energy monitor (owned by main.c)
 */

#ifndef MONITOR_H_
#define MONITOR_H_

#include <stdint.h>
#include "measure.h"

enum { DISP_AUTO = 0, DISP_V, DISP_I, DISP_P, DISP_E, DISP_PF };

typedef struct {
	meas_result_t last;        /* latest calibrated result                         */
	uint8_t  have_last;
	float    energy_Wh;        /* integrated real power since power-up / RESET     */
	uint32_t t0_ticks;         /* tick count at power-up / RESET                   */
	uint8_t  have_mm;          /* min/max valid                                     */
	uint32_t pmin_mW, pmax_mW;
	uint8_t  disp_mode;        /* DISP_AUTO cycles V, I, P, E                       */
	uint8_t  axis_dirty;       /* send the A line at the next report                */
	uint16_t freq_dHz;         /* measured mains frequency, 0.1 Hz units            */
	uint8_t  doom_request;     /* DOOM command received: main starts the game       */
} monitor_t;

extern monitor_t mon;

/* Handles one command line from the app (already without '\n'). */
void commands_handle(char *line);

#endif /* MONITOR_H_ */
