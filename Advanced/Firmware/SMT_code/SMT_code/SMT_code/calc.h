/*
 * calc.h
 *
 * Created: 18/10/2026 10:16:24 pm
 *  Author: thoma
 */ 


#ifndef CALC_H_
#define CALC_H_


#include <stdint.h>

// Calculates RMS voltage, real power and maximum current from the converted
// data (REAL_MV / REAL_MA, see convert.h).
// Call after convert_to_real(), once per capture.
//
// The buffers hold exactly one period, so plain averages over all points are
// exact (equivalent to the trapezium rule over the full period).

// Phase correction between the voltage and current filters, in buffer points.
// One point is 1/160 of a period = 2.25 degrees, at any frequency.
// Positive: the current channel's filter delays more than the voltage one, so
// current sample k + CALC_I_SHIFT is paired with voltage sample k.
// Find it with a purely resistive load: pick the shift that makes
// P closest to Vrms * Irms. Leave at 0 if the filters match.
#define CALC_I_SHIFT  0

typedef struct {
	uint16_t vrms_mv;    // RMS load voltage, mV (includes any DC from the source)
	int32_t  power_mw;   // real power, mW (negative = power flowing back)
	uint16_t imax_ma;    // peak |current|, averaged with its two neighbours, mA
} calc_result_t;

// Fill *r from REAL_MV / REAL_MA. Takes about 10 ms at 2 MHz.
void calc_run(calc_result_t *r);



#endif /* CALC_H_ */