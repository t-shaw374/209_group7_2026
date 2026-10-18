/*
 * measure.c  -  see measure.h
 */

#include <math.h>
#include "common.h"
#include "measure.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void meas_clear(meas_sums_t *s)
{
	s->n = 0;
	s->sv = s->svv = s->si = s->sii = s->svi = s->svx = 0;
}

void meas_accumulate(meas_sums_t *s, const uint16_t *v, const uint16_t *i, uint8_t n)
{
	/* Centre on mid-scale so the sums stay small (fits int32 for WINDOWS*N_PAIRS <= 400). */
	int16_t vk = (int16_t)v[0] - 512;
	for (uint8_t k = 0; k < n; k++) {
		int16_t vn = (int16_t)v[k + 1] - 512;
		int16_t ik = (int16_t)i[k] - 512;
		int16_t vx = vk + vn;              /* 2 x interpolated voltage at the I sample */

		s->sv  += vk;
		s->svv += (int32_t)vk * vk;
		s->si  += ik;
		s->sii += (int32_t)ik * ik;
		s->svx += vx;
		s->svi += (int32_t)vx * ik;

		vk = vn;
	}
	s->n += n;
}

void meas_compute(const meas_sums_t *s, const meas_cal_t *cal, meas_result_t *r)
{
	const float lsb = ADC_VREF / 1024.0f;                       /* volts per count   */
	/* interpolation correction: 1 / cos(2*pi*f*d) */
	const float k_interp = 1.0f / cosf(2.0f * (float)M_PI * MAINS_HZ * (SAMPLE_US * 1e-6f));

	if (s->n == 0) {
		r->vrms = r->irms = r->p = r->s = r->pf = 0;
		return;
	}
	const float n = (float)s->n;

	/* mean and variance (AC power) of each channel, in counts */
	float mv = s->sv / n, mi = s->si / n;
	float var_v = s->svv / n - mv * mv;
	float var_i = s->sii / n - mi * mi;
	/* covariance of interpolated v with i: mean(vx*i)/2 - mean(vx)/2 * mean(i) */
	float cov = (s->svi / n - (s->svx / n) * mi) * 0.5f;

	r->var_v  = var_v;
	r->var_i  = var_i;
	r->mean_v = mv + 512.0f;
	r->mean_i = mi + 512.0f;

	/* remove the noise floor measured with ZERO (noise adds in quadrature) */
	float ac_v = var_v - cal->noise_vv;
	float ac_i = var_i - cal->noise_ii;
	if (ac_v < 0) ac_v = 0;
	if (ac_i < 0) ac_i = 0;

	r->vrms = sqrtf(ac_v) * lsb / V_RATIO * cal->cal_v;
	r->irms = sqrtf(ac_i) * lsb / I_RATIO * cal->cal_i;

	float p = cov * k_interp * lsb * lsb / (V_RATIO * I_RATIO) * cal->cal_v * cal->cal_i;
	if (p < 0) p = -p;          /* sign only depends on which way the shunt is wired */

	if (r->irms * 1000.0f < NO_LOAD_mA) {
		r->irms = 0;
		p = 0;
	}
	r->p = p;
	r->s = r->vrms * r->irms;
	r->pf = (r->s > 0.001f) ? p / r->s : 0.0f;
	if (r->pf > 1.0f) r->pf = 1.0f;
	if (r->p > r->s) r->p = r->s;    /* rounding/noise: P can't exceed S */
}
