/*
 * measure.h  -  RMS, real power, power factor from raw ADC samples
 *
 * Pure C (no hardware access) so it can be tested on a PC.
 *
 * Samples are taken alternately V0, I0, V1, I1, ... V(N), each SAMPLE_US apart.
 * Because V and I are never sampled at the same instant, the voltage at the time
 * of each current sample is estimated by linear interpolation:
 *     v_k' = (v_k + v_(k+1)) / 2
 * For a 500 Hz sinusoid this interpolation scales the voltage by cos(w*d),
 * d = SAMPLE_US, which is corrected exactly with a constant (lecture method).
 */

#ifndef MEASURE_H_
#define MEASURE_H_

#include <stdint.h>

/* Running sums over one or more capture windows (integer, exact). */
typedef struct {
	uint16_t n;        /* number of current samples (= interpolated voltage samples) */
	int32_t  sv, svv;  /* sum v, sum v^2              (v = ADC - 512)               */
	int32_t  si, sii;  /* sum i, sum i^2              (i = ADC - 512)               */
	int32_t  svi;      /* sum (v_k + v_k+1) * i_k     (interpolated v, times 2)     */
	int32_t  svx;      /* sum (v_k + v_k+1)                                          */
} meas_sums_t;

/* Calibration / noise-floor settings. */
typedef struct {
	float cal_v;       /* multiplier from CALV (1.0 = design value)                */
	float cal_i;       /* multiplier from CALI                                     */
	float noise_vv;    /* ADC noise variance on V with AC off (counts^2), ZERO     */
	float noise_ii;    /* ADC noise variance on I with AC off (counts^2), ZERO     */
} meas_cal_t;

typedef struct {
	float vrms;        /* V */
	float irms;        /* A */
	float p;           /* W  (real power) */
	float s;           /* VA (apparent power) */
	float pf;          /* 0..1 */
	float var_v, var_i;/* raw AC variance in counts^2 (for ZERO / STATUS) */
	float mean_v, mean_i; /* raw DC level in ADC counts (for STATUS) */
} meas_result_t;

void meas_clear(meas_sums_t *s);

/* Add one capture window: v[0..n] (n+1 values) and i[0..n-1] (n values). */
void meas_accumulate(meas_sums_t *s, const uint16_t *v, const uint16_t *i, uint8_t n);

void meas_compute(const meas_sums_t *s, const meas_cal_t *cal, meas_result_t *r);

#endif /* MEASURE_H_ */
