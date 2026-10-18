/*
 * sampler.c  -  see sampler.h
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "sampler.h"

#define ADMUX_V  ((1 << REFS0) | 0)   /* AVCC reference, ADC0 = Vmeas */
#define ADMUX_I  ((1 << REFS0) | 1)   /* AVCC reference, ADC1 = Imeas */

#define T1_TOP   ((uint16_t)((F_CPU / 1000000UL) * SAMPLE_US) - 1)   /* 399 at 2 MHz */

static uint16_t *volatile s_v;
static uint16_t *volatile s_i;
static volatile uint8_t  s_idx;
static volatile uint8_t  s_done;
static volatile uint16_t s_zc;

void sampler_init(void)
{
	/* ADC: AVCC ref, prescaler 16 -> 125 kHz (Lab 4), auto-trigger on Timer1 compare B */
	ADMUX  = ADMUX_V;
	ADCSRA = (1 << ADEN) | (1 << ADPS2);
	ADCSRB = (1 << ADTS2) | (1 << ADTS0);           /* 101 = Timer/Counter1 Compare Match B */
	DIDR0  = (1 << ADC0D) | (1 << ADC1D) | (1 << ADC2D); /* analogue pins: digital buffers off */

	/* first conversion after enabling is a long one; get it out of the way */
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC)) { ; }

	/* Timer1: CTC (TOP = OCR1A), stopped until a capture starts */
	TCCR1A = 0;
	TCCR1B = (1 << WGM12);
	OCR1A  = T1_TOP;
	OCR1B  = T1_TOP;

	/* INT0 (PD2) on rising edges of the zero-crossing comparator */
	DDRD  &= ~(1 << PD2);
	EICRA  = (EICRA & ~((1 << ISC01) | (1 << ISC00))) | (1 << ISC01) | (1 << ISC00);
	EIFR   = (1 << INTF0);
	EIMSK |= (1 << INT0);
}

ISR(INT0_vect)
{
	s_zc++;
}

/* One conversion finished. Store it, select the other channel for the next one.
 * Kept short: the next Timer1 trigger is ~90 us away. */
ISR(ADC_vect)
{
	uint16_t r = ADC;
	uint8_t  k = s_idx;

	TIFR1 = (1 << OCF1B);            /* re-arm: the trigger is the flag's rising edge */

	if (k & 1) {
		s_i[k >> 1] = r;
		ADMUX = ADMUX_V;
	} else {
		s_v[k >> 1] = r;
		ADMUX = ADMUX_I;
	}

	if (++k >= 2 * N_PAIRS + 1) {    /* V0 I0 V1 I1 ... V(N): 2N+1 conversions */
		TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));
		ADCSRA &= ~((1 << ADATE) | (1 << ADIE));
		s_done = 1;
	}
	s_idx = k;
}

void sampler_capture(uint16_t *v, uint16_t *i)
{
	s_v = v;
	s_i = i;
	s_idx = 0;
	s_done = 0;

	ADMUX = ADMUX_V;
	TCNT1 = 0;
	TIFR1 = (1 << OCF1B);
	ADCSRA |= (1 << ADIF);                       /* clear any stale flag */
	ADCSRA |= (1 << ADATE) | (1 << ADIE);
	TCCR1B |= (1 << CS10);                       /* start Timer1, no prescaler */

	while (!s_done) { ; }                        /* 2N+1 conversions = ~40 ms */
}

uint16_t sampler_read_channel(uint8_t channel)
{
	ADMUX = (1 << REFS0) | (channel & 0x0F);
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC)) { ; }
	uint16_t r = ADC;
	ADMUX = ADMUX_V;
	return r;
}

uint16_t sampler_take_zc_count(void)
{
	uint16_t n;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		n = s_zc;
		s_zc = 0;
	}
	return n;
}
