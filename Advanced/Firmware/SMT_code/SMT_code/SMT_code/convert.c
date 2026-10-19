/*
 * convert.c
 *
 * Created: 18/10/2026 10:01:16 pm
 *  Author: thoma
 */ 
#include <avr/io.h>
#include "convert.h"

#define OFFSET_SAMPLES  16   // conversions averaged for the offset (sum fits uint16_t)

// Scale factors in "Q10" form: value per ADC count x 1024, so
//   real = (counts * K_Q10) / 1024
//
// Voltage: K = Aref / (1024 * G),  G = RBOT/(RTOP+RBOT) * RF/RIN
//          K_Q10 = Aref_mV * (RTOP+RBOT) * RIN / (RBOT * RF)
//          = 3300 * 285 * 82 / (15 * 100) = 51414   -> 50.21 mV per count
//
// Current: K = Aref / (1024 * Rshunt * G),  mV / ohm = mA
//          K_Q10 = Aref_mV * 1000 * RIN / (Rshunt_mohm * RF)
//          = 3300 * 1000 * 10 / (500 * 33) = 2000   -> 1.953 mA per count
#define V_MV_Q10  (((uint32_t)AREF_MV * (VDIV_RTOP + VDIV_RBOT) * VAMP_RIN \
+ (uint32_t)VDIV_RBOT * VAMP_RF / 2)                   \
/ ((uint32_t)VDIV_RBOT * VAMP_RF))

#define I_MA_Q10  (((uint32_t)AREF_MV * 1000UL * IAMP_RIN                  \
+ (uint32_t)SHUNT_MOHM * IAMP_RF / 2)                  \
/ ((uint32_t)SHUNT_MOHM * IAMP_RF))

static uint16_t last_offset16;   // offset reference, ADC counts x 16

uint16_t convert_measure_offset(void)
{
	uint8_t admux_save  = ADMUX;
	uint8_t adcsra_save = ADCSRA;

	// Manual conversions: auto-trigger and the ADC interrupt off, so the
	// capture ISR in adc.c doesn't run and grab these results.
	// Writing 1 to ADIF clears any old flag.
	ADCSRA = (adcsra_save & ~((1 << ADATE) | (1 << ADIE))) | (1 << ADIF);
	ADMUX  = (admux_save & 0xF0) | OFFSET_CH;

	uint16_t sum = 0;
	for (uint8_t k = 0; k <= OFFSET_SAMPLES; k++) {
		ADCSRA |= (1 << ADSC);                   // start one conversion
		while (ADCSRA & (1 << ADSC)) ;           // ~104 us
		if (k != 0) {                            // first one after the mux switch
			sum += ADC;                          // is thrown away (settling)
		}
	}

	ADMUX  = admux_save;
	// Restore auto-trigger and the interrupt, and clear ADIF at the same time.
	// Otherwise the flag from the last manual conversion would fire the capture
	// ISR as soon as ADIE is back on, and it would write into adc_buf.
	ADCSRA = adcsra_save | (1 << ADIF);

	return sum;                                  // 16 conversions = average x 16
}

// Clamp to the int16_t range
static int16_t clamp16(int32_t x)
{
	if (x >  32767) x =  32767;
	if (x < -32768) x = -32768;
	return (int16_t)x;
}

// d16: (sample - zero) in 1/16 counts, |d16| <= 16368.
// Returns d16 * K_Q10 / (1024 * 16), rounded, clamped to int16_t.
// |d16 * K| <= 16368 * 51414 = 8.4e8, fits int32_t.
// >> on a negative int32_t is an arithmetic shift in avr-gcc (rounds down),
// so adding half before the shift rounds to nearest for both signs.
static int16_t scale(int32_t d16, int32_t k_q10)
{
	return clamp16((d16 * k_q10 + (1L << 13)) >> 14);
}

#if V_INCLUDE_SHUNT
// Shunt voltage drop in mV for a current in mA: i * Rshunt_mohm / 1000,
// rounded to nearest for either sign. |i * 500| <= 1.6e7, fits int32_t.
static int32_t shunt_drop_mv(int16_t i_ma)
{
	int32_t x = (int32_t)i_ma * SHUNT_MOHM;
	return (x >= 0) ? (x + 500) / 1000 : (x - 500) / 1000;
}
#endif

void convert_to_real(void)
{
	uint16_t off16 = convert_measure_offset();
	last_offset16 = off16;

	int32_t zero_v16 = (int32_t)off16 + V_ZERO_CORR16;
	int32_t zero_i16 = (int32_t)off16 + I_ZERO_CORR16;

	volatile int16_t *v = REAL_MV;
	volatile int16_t *c = REAL_MA;

	for (uint16_t k = 0; k < ADC_POINTS; k++) {
		int32_t rv = (int32_t)adc_buf[ADC_VOLT][k] * 16;   // raw x 16
		int32_t ri = (int32_t)adc_buf[ADC_CURR][k] * 16;
		int16_t vk = scale(rv - zero_v16, V_MV_Q10);      // load voltage, mV
		int16_t ik = scale(ri - zero_i16, I_MA_Q10);      // current, mA
#if V_INCLUDE_SHUNT
		// Same instant in both buffers, so this is the source voltage at
		// that instant: Vsource = Vload + i * Rshunt
		v[k] = clamp16((int32_t)vk + shunt_drop_mv(ik));
#else
		v[k] = vk;
#endif
		c[k] = ik;
	}
}

uint16_t convert_offset_mv(void)
{
	// counts x 16 -> mV: off16 * Aref / (1024 * 16), rounded
	return (uint16_t)(((uint32_t)last_offset16 * AREF_MV + 8192) >> 14);
}