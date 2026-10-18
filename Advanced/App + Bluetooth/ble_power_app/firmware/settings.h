/*
 * settings.h  -  calibration and user settings, kept in EEPROM across power cycles
 */

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <stdint.h>
#include "measure.h"

typedef struct {
	uint16_t   magic;          /* SETTINGS_MAGIC when valid */
	meas_cal_t cal;            /* CALV / CALI multipliers, ZERO noise floor */
	uint16_t   rate_ms;        /* RATE: report period */
	uint32_t   axis_max_mW;    /* AXIS: top of the app's power graph */
} settings_t;

extern settings_t settings;

void settings_load(void);      /* loads from EEPROM, or defaults if blank/invalid */
void settings_save(void);
void settings_defaults(void);  /* RAM only; call settings_save() to keep */

#endif /* SETTINGS_H_ */
