// Equivalent-time sampling (ETS) of voltage and current, ATmega328P @ 2 MHz
//
// - ADC clock = 2 MHz / 16 = 125 kHz  (conversion = 13 ADC clocks = 104 us)
// - Timer0 CTC, compare match A every 250 ticks (125 us) auto-triggers the ADC
// - Zero-crossing signal on PC0 (ADC0 pin, digital input) via pin-change interrupt.
// - One capture = one signal period. The first rising edge starts the timer.
//   Sampling stops after the 16th sample or at the next rising edge, whichever
//   comes first. So normally 16 samples, but 15 if the period gets a little
//   shorter (the missing slot is left as ADC_EMPTY).
// - Each offset index delays the first sample by another 25 ticks (12.5 us), and
//   samples are stored at index n * 10 + offset, so 10 offsets interleave into
//   one finer-grained period of 160 points.
// - For each offset: capture voltage in one period, then current in the next.
//   Both start from the same zero-crossing edge, so index i is the same instant
//   in both buffers.
//
// Signals to be measured: ADC1 (PC1, voltage) and ADC2 (PC2, current).

#include <avr/io.h>
#include <avr/interrupt.h>
#include "adc.h"

#define T_SAMPLE  250    // timer ticks between samples (125 us @ 2 MHz)
#define STEP      25     // timer ticks per offset step (12.5 us)

volatile uint16_t adc_buf[2][ADC_POINTS];

static const uint8_t mux_ch[2] = { ADC_CH_V, ADC_CH_I };

static volatile uint16_t * volatile row;   // buffer row being filled
static volatile uint8_t cycle;     // current offset index 0..ADC_OFFSETS-1
static volatile uint8_t n;         // samples stored so far in this capture
static volatile uint8_t armed;     // waiting for the zero crossing that starts a capture
static volatile uint8_t running;   // capturing, waiting for the crossing that ends it
static volatile uint8_t done;      // capture finished

// Zero crossing on PC0, rising edge only.
// 1st edge: preset the timer so the first compare match (first ADC trigger)
//           comes (5 + 25*cycle) ticks later, and start it.
// 2nd edge: end of the period, stop the timer.
ISR(PCINT1_vect)
{
	if (!(PINC & (1 << PC0))) {
		return;                                  // falling edge, ignore
	}
	if (armed) {
		armed   = 0;
		n       = 0;
		running = 1;
		TCNT0   = (T_SAMPLE - 6) - STEP * cycle; // 244 - 25*cycle
		TIFR0   = (1 << OCF0A);                  // clear stale compare flag
		TCCR0B  = (1 << CS00);                   // start timer, prescaler 1
		} else if (running) {
		TCCR0B  = 0;                             // stop timer, no more triggers
		running = 0;
		done    = 1;
	}
}

// Empty compare-match ISR. Its only job is to make the hardware clear OCF0A
// right after every match. The ADC triggers on the rising edge of that flag, so
// the flag must be low again before the next match, otherwise that trigger is lost.
EMPTY_INTERRUPT(TIMER0_COMPA_vect)

// Conversion complete: store the result at its interleaved position.
// Offset 0 fills row[0], row[10], row[20] ..., offset 1 fills row[1], row[11] ...
ISR(ADC_vect)
{
	if (n < ADC_SAMPLES) {
		row[n * ADC_OFFSETS + cycle] = ADC;
		n++;
		if (n == ADC_SAMPLES) {
			// 16 samples taken: stop here, even if the period isn't over yet
			TCCR0B  = 0;
			running = 0;
			done    = 1;
		}
	}
}

void adc_init(void)
{
	// ADC: AVcc reference, prescaler 16 -> 125 kHz,
	// auto-trigger on Timer0 compare match A
	ADMUX  = (1 << REFS0);
	adc_set_channel(ADC_CH_V);            // default: voltage channel
	ADCSRB = (1 << ADTS1) | (1 << ADTS0);

	// Dummy conversion: the first conversion after ADEN takes 25 ADC clocks
	// (200 us) instead of 13, which is longer than the 125 us trigger spacing
	// and makes the second trigger get lost. Get it out of the way here.
	ADCSRA = (1 << ADEN) | (1 << ADPS2);          // enable, prescaler 16
	ADCSRA |= (1 << ADSC);                        // start one conversion
	while (ADCSRA & (1 << ADSC)) ;                // wait for it to finish
	ADCSRA |= (1 << ADIF);                        // clear the done flag (write 1)

	// Now switch on auto-trigger and the conversion-complete interrupt
	ADCSRA = (1 << ADEN) | (1 << ADATE) | (1 << ADIE) | (1 << ADPS2);

	// Timer0: CTC mode, TOP = OCR0A, started later by the zero-crossing ISR
	TCCR0A = (1 << WGM01);
	OCR0A  = T_SAMPLE - 1;
	TIMSK0 = (1 << OCIE0A);               // compare ISR clears the flag (see above)

	// Pin change interrupt on PC0 (PCINT8)
	PCMSK1 = (1 << PCINT8);
	PCICR  = (1 << PCIE1);
}

// Select the input channel for the next capture (1..7, except 0).
// Call only between captures, not while a capture is running.
// ADC0 (PC0) is the zero-crossing input, so it can't be selected here.
void adc_set_channel(uint8_t ch)
{
	if (ch > 7 || ch == 0) {
		return;                              // invalid, keep current channel
	}
	ADMUX = (ADMUX & 0xF0) | ch;             // keep REFS/ADLAR, change MUX3:0
	if (ch <= 5) {
		DIDR0 |= (1 << ch);                  // switch off digital buffer on that pin
	}
}

void adc_capture(void)
{
	// mark everything empty, so slots that don't get captured are recognisable
	for (uint8_t s = 0; s < 2; s++) {
		for (uint8_t i = 0; i < ADC_POINTS; i++) {
			adc_buf[s][i] = ADC_EMPTY;
		}
	}

	for (uint8_t k = 0; k < ADC_OFFSETS; k++) {
		for (uint8_t s = 0; s < 2; s++) {    // 0 = voltage, 1 = current
			adc_set_channel(mux_ch[s]);
			row   = adc_buf[s];
			cycle = k;
			done  = 0;
			armed = 1;
			while (!done) ;                  // one period

			// A conversion started just before the last crossing may still be
			// running. Wait for it and for its ISR so its sample is stored
			// before the next capture changes row/cycle/n.
			while (ADCSRA & (1 << ADSC)) ;
			while (ADCSRA & (1 << ADIF)) ;
		}
	}
}