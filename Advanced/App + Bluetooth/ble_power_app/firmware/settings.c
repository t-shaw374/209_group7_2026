/*
 * settings.c  -  see settings.h
 */

#include <avr/eeprom.h>
#include "common.h"
#include "settings.h"

#define SETTINGS_MAGIC 0x2097      /* change if settings_t changes layout */

settings_t settings;
static settings_t EEMEM ee_settings;

void settings_defaults(void)
{
	settings.magic        = SETTINGS_MAGIC;
	settings.cal.cal_v    = 1.0f;
	settings.cal.cal_i    = 1.0f;
	settings.cal.noise_vv = 0.0f;
	settings.cal.noise_ii = 0.0f;
	settings.rate_ms      = DEFAULT_RATE_MS;
	settings.axis_max_mW  = DEFAULT_AXIS_MAX_mW;
}

void settings_load(void)
{
	eeprom_read_block(&settings, &ee_settings, sizeof settings);
	if (settings.magic != SETTINGS_MAGIC
	    || !(settings.cal.cal_v > 0.5f && settings.cal.cal_v < 2.0f)
	    || !(settings.cal.cal_i > 0.5f && settings.cal.cal_i < 2.0f)
	    || settings.rate_ms < 250 || settings.rate_ms > 10000) {
		settings_defaults();
	}
}

void settings_save(void)
{
	eeprom_update_block(&settings, &ee_settings, sizeof settings);  /* only rewrites changed bytes */
}
