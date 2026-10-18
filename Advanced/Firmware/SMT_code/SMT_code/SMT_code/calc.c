/*
 * calc.c
 *
 * Created: 18/10/2026 10:15:55 pm
 *  Author: thoma
 */ 
#include "calc.h"
#include "convert.h"

#define N  ADC_POINTS

// Integer square root, rounded to the nearest integer.
// x up to 32768^2 (result up to 32768).
static uint16_t isqrt32(uint32_t x)
{
	uint32_t res = 0;
	uint32_t bit = 1UL << 30;                    // highest power of 4 <= 2^32

	while (bit > x) {
		bit >>= 2;
	}
	while (bit) {
		if (x >= res + bit) {
			x  -= res + bit;
			res = (res >> 1) + bit;
			} else {
			res >>= 1;
		}
		bit >>= 2;
	}
	// x is now the remainder (input - res^2). Round up if the input is past
	// (res + 0.5)^2 = res^2 + res + 0.25, i.e. remainder > res.
	if (x > res) {
		res++;
	}
	return (uint16_t)res;
}

// |x| as unsigned, works for -32768 too
static uint16_t mag(int16_t x)
{
	return (x < 0) ? (uint16_t)(-(int32_t)x) : (uint16_t)x;
}

// Wrap an index into 0..N-1 (the buffer is one full period, so it's circular)
static uint16_t wrap(int16_t k)
{
	while (k < 0) {
		k += N;
	}
	while (k >= N) {
		k -= N;
	}
	return (uint16_t)k;
}

void calc_run(calc_result_t *r)
{
	volatile int16_t *v = REAL_MV;
	volatile int16_t *c = REAL_MA;

	// ---- RMS voltage and real power: one pass ------------------------------
	// mV^2 per point is up to 1.07e9, and 160 of them overflow 32 bits,
	// so the sums are 64-bit.
	uint64_t svv = 0;                            // sum of v^2,  mV^2
	int64_t  svi = 0;                            // sum of v*i,  mV*mA = uW
	uint16_t j   = wrap(CALC_I_SHIFT);           // current index paired with k

	for (uint16_t k = 0; k < N; k++) {
		int16_t vk = v[k];
		int16_t ik = c[j];
		svv += (uint32_t)((int32_t)vk * vk);
		svi += (int32_t)vk * ik;
		if (++j == N) {
			j = 0;
		}
	}

	// Vrms = sqrt(mean of v^2). The mean fits 32 bits (max 1.07e9).
	uint32_t mean_sq = (uint32_t)((svv + N / 2) / N);
	r->vrms_mv = isqrt32(mean_sq);

	// P = mean of v*i, in uW, then / 1000 for mW. Rounded to nearest for
	// either sign (C division truncates towards zero).
	const int64_t den = (int64_t)N * 1000;
	if (svi >= 0) {
		r->power_mw = (int32_t)((svi + den / 2) / den);
		} else {
		r->power_mw = (int32_t)((svi - den / 2) / den);
	}

	// ---- Maximum current ---------------------------------------------------
	// Largest |i|, then averaged with the points either side to cut noise.
	// Neighbours wrap around the ends (one full period). No phase shift here:
	// the peak value doesn't depend on its timing.
	uint16_t pk = 0;
	uint16_t m  = 0;
	for (uint16_t k = 0; k < N; k++) {
		uint16_t a = mag(c[k]);
		if (a > pk) {
			pk = a;
			m  = k;
		}
	}
	uint32_t s3 = (uint32_t)mag(c[wrap((int16_t)m - 1)]) + pk
	+ mag(c[wrap((int16_t)m + 1)]);
	r->imax_ma = (uint16_t)((s3 + 1) / 3);
}