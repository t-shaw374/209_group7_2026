// Equivalent-time sampling (ETS) of voltage and current, ATmega328PB @ 2 MHz
//
// - ADC clock = 2 MHz / 16 = 125 kHz  (conversion = 13 ADC clocks = 104 us)
// - Before every capture the signal period is measured over ADC_PER_COUNT (8)
//   periods with Timer4, and all timing below is derived from that:
//     t_sample = period / 16        (Timer1 ticks between samples)
//     offset k = k * t_sample / 10  (k = 0..9, the 10 interleaved phases)
//   At 500 Hz this gives t_sample = 250 ticks (125 us) and 25-tick (12.5 us) steps.
// - Timer1 CTC (mode 4, TOP = OCR1A = t_sample - 1), compare match B auto-triggers
//   the ADC. Timer0 is not used here.
// - Zero-crossing signal on PC0 (ADC0 pin, digital input) via pin-change interrupt.
// - One capture = exactly 16 samples = one period. A rising edge starts the
//   timer, and the timer stops itself after the 16th trigger. The next capture
//   starts on the very next rising edge, so a capture takes one period. For the
//   last offsets (about 7..9) the 16th trigger lands too close to the next edge,
//   so that edge is skipped and those captures take two periods.
// - Samples are stored at index n * 10 + offset, so 10 offsets interleave into
//   one finer-grained period of 160 points.
// - Capture order: V offset 0, I offset 0, V offset 1, I offset 1 ... Both signals
//   at one offset start from a zero-crossing edge, so index i is the same instant
//   in both buffers.
// - The channel is switched in the edge interrupt, just before the timer starts.
//   The ADC locks its channel once a conversion has started, so switching while
//   the last conversion of the previous capture is still running is safe.
//
// Signals to be measured: ADC1 (PC1, voltage) and ADC2 (PC2, current).

#include <avr/io.h>
#include <avr/interrupt.h>
#include "adc.h"

#define T_FIRST       8      // timer ticks from start to the first trigger at offset 0
#define T_SAMPLE_MIN  220    // shortest allowed sample spacing (104 us conversion + margin)
#define T_SAMPLE_MAX  455    // longest the period measurement can deliver (~3.6 ms / 16)
#define CAPTURES      (2 * ADC_OFFSETS)

// WGM12 (CTC mode bit) lives in TCCR1B together with the clock select bits,
// so stopping the timer must keep WGM12 set. Never write TCCR1B = 0.
#define T1_STOPPED  (1 << WGM12)                 // CTC mode 4, no clock
#define T1_RUNNING  ((1 << WGM12) | (1 << CS10)) // CTC mode 4, prescaler 1

#define PER_CS      (1 << CS40)                  // Timer4 normal mode, prescaler 1 (0.5 us)

volatile uint16_t adc_buf[2][ADC_POINTS];

static const uint8_t mux_ch[2] = { ADC_CH_V, ADC_CH_I };

// Timer1 preload for each offset, computed from the measured period before
// every capture (a division is far too slow for the edge ISR at 2 MHz).
static uint16_t t_start[ADC_OFFSETS];
static uint16_t last_ticks;            // last good period measurement (8 periods)

static volatile uint8_t  start_idx;    // captures started (edge ISR)
static volatile uint8_t  trig;         // triggers so far in the current capture (compare ISR)
static volatile uint8_t  store_idx;    // capture whose samples are being stored (ADC ISR)
static volatile uint8_t  n;            // samples stored so far in that capture (ADC ISR)
static volatile uint16_t * volatile wp;// where the next result goes (ADC ISR)
static volatile uint8_t  running;      // a capture sequence is in progress
static volatile uint8_t  done;         // all captures stored

static volatile uint8_t  per_active;   // period measurement in progress
static volatile uint8_t  per_edges;    // rising edges seen so far
static volatile uint16_t per_t0;       // Timer4 count at the first edge
static volatile uint16_t per_ticks;    // ticks for ADC_PER_COUNT periods
static volatile uint8_t  per_ovf;      // Timer4 overflowed before the last edge

// Zero crossing on PC0, rising edge only.
// During a period measurement it only timestamps edges with Timer4.
// During a capture it starts the next capture, unless all are started, or the
// timer is still running. The timer is still running when the 16th trigger of
// the previous capture comes too close to this edge (offsets ~7..9). That edge
// is skipped and the capture starts on the next one, so those captures take two
// periods. Nothing is corrupted.
// (This ISR needs ~36 us from the edge to starting the timer. That delay is the
// same every time, so it only shifts all samples by a constant.)
// TCNT1/TCNT4 are 16-bit accesses; they are safe here because this ISR runs with
// interrupts off and no other ISR touches 16-bit Timer1/Timer4 registers.
ISR(PCINT1_vect)
{
	if (!(PINC & (1 << PC0))) {
		return;                                      // falling edge, ignore
	}

	if (per_active) {
		uint16_t t = TCNT4;                          // same spot for every edge, latency cancels
		uint8_t  e = per_edges++;
		if (e == 0) {
			per_t0 = t;
			} else if (e == ADC_PER_COUNT) {
			per_ticks  = t - per_t0;
			per_ovf    = TIFR4 & (1 << TOV4);        // wrapped somewhere in between
			per_active = 0;
		}
		return;
	}

	if (running && start_idx < CAPTURES && TCCR1B == T1_STOPPED) {
		uint8_t i = start_idx++;
		ADMUX  = (ADMUX & 0xF0) | mux_ch[i & 1];     // V on even, I on odd captures
		TCNT1  = t_start[i >> 1];
		trig   = 0;
		TIFR1  = (1 << OCF1B);                       // clear stale compare flag
		TCCR1B = T1_RUNNING;                         // start timer, prescaler 1
	}
}

// Runs right after every compare match B. Entering it makes the hardware clear
// OCF1B, which the ADC needs (it triggers on the rising edge of that flag, so the
// flag has to be low again before the next match). Also stops the timer after
// the 16th trigger.
// ISR_NOBLOCK re-enables interrupts at entry, so the zero-crossing interrupt can
// pre-empt this one. Otherwise it could be delayed by up to ~20 us and the next
// capture would start late.
ISR(TIMER1_COMPB_vect, ISR_NOBLOCK)
{
	if (++trig == ADC_SAMPLES) {
		TCCR1B = T1_STOPPED;
	}
}

// Conversion complete: store the result at its interleaved position.
// Offset 0 fills row[0], row[10], row[20] ..., offset 1 fills row[1], row[11] ...
// ISR_NOBLOCK for the same reason as above (this one takes ~40 us at 2 MHz, and
// the last conversion of a capture finishes right around the next zero crossing).
ISR(ADC_vect, ISR_NOBLOCK)
{
	*wp = ADC;
	if (++n == ADC_SAMPLES) {
		n = 0;
		if (++store_idx == CAPTURES) {
			running = 0;
			done    = 1;
			} else {
			wp = &adc_buf[store_idx & 1][store_idx >> 1];
		}
		} else {
		wp += ADC_OFFSETS;
	}
}

void adc_init(void)
{
	// ADC: Aref reference, prescaler 16 -> 125 kHz,
	// auto-trigger on Timer1 compare match B
	ADMUX  = (0);
	adc_set_channel(ADC_CH_V);
	DIDR0 |= (1 << ADC_CH_V) | (1 << ADC_CH_I);   // digital buffers off on both signal pins
	ADCSRB = (1 << ADTS2) | (1 << ADTS0);         // trigger source: Timer1 compare match B

	// Dummy conversion: the first conversion after ADEN takes 25 ADC clocks
	// (200 us) instead of 13, which is longer than the trigger spacing
	// and makes the second trigger get lost. Get it out of the way here.
	ADCSRA = (1 << ADEN) | (1 << ADPS2);          // enable, prescaler 16
	ADCSRA |= (1 << ADSC);                        // start one conversion
	while (ADCSRA & (1 << ADSC)) ;                // wait for it to finish
	ADCSRA |= (1 << ADIF);                        // clear the done flag (write 1)

	// Now switch on auto-trigger and the conversion-complete interrupt
	ADCSRA = (1 << ADEN) | (1 << ADATE) | (1 << ADIE) | (1 << ADPS2);

	// Timer1: CTC mode 4, stopped. OCR1A/OCR1B are set per capture from the
	// measured period (set_timing).
	TCCR1A = 0;
	TCCR1B = T1_STOPPED;
	TIMSK1 = (1 << OCIE1B);

	// Timer4: normal mode, stopped. Used only by adc_measure_period().
	TCCR4A = 0;
	TCCR4B = 0;

	// Pin change interrupt on PC0 (PCINT8)
	PCMSK1 = (1 << PCINT8);
	PCICR  = (1 << PCIE1);
}

// Select the input channel (1..7, except 0).
// Call only while no capture is running.
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

uint16_t adc_measure_period(void)
{
	if (running) {
		return 0;                                // capture in progress, not allowed
	}

	TCCR4B = 0;                                  // stop Timer4
	TCCR4A = 0;                                  // normal mode
	TCNT4  = 0;
	TIFR4  = (1 << TOV4);                        // clear overflow flag (write 1)

	per_edges  = 0;
	per_ovf    = 0;
	per_active = 1;                              // from here the edge ISR timestamps edges
	TCCR4B     = PER_CS;                         // start counting from 0

	while (per_active) {
		if (TIFR4 & (1 << TOV4)) {               // 32.7 ms gone: no signal or too slow
			per_active = 0;
			TCCR4B = 0;
			return 0;
		}
	}

	TCCR4B = 0;
	if (per_ovf) {
		return 0;
	}
	return per_ticks;
}

// Derive all capture timing from one sample spacing ts (Timer1 ticks).
// Offset k starts k * ts / 10 later, rounded to the nearest tick, so the 10
// phases split each sample interval evenly. The first trigger of offset k comes
// T_FIRST + offset_k ticks after the timer starts, so the preload is
// ts - T_FIRST - offset_k (always >= 0 for ts >= T_SAMPLE_MIN).
// Timer1 must be stopped.
static void set_timing(uint16_t ts)
{
	OCR1A = ts - 1;                              // TOP
	OCR1B = ts - 1;                              // trigger at TOP, like before
	for (uint8_t k = 0; k < ADC_OFFSETS; k++) {
		uint16_t off = (k * ts + ADC_OFFSETS / 2) / ADC_OFFSETS;
		t_start[k] = ts - T_FIRST - off;
	}
}

uint8_t adc_capture(void)
{
	uint16_t ticks = adc_measure_period();       // 8 periods, 0.5 us ticks
	if (ticks == 0) {
		return 0;                                // no signal / too slow
	}

	// period / 16 samples = (ticks / 8) / 16 = ticks / 128, rounded
	uint16_t ts = (ticks + 64) >> 7;
	if (ts < T_SAMPLE_MIN || ts > T_SAMPLE_MAX) {
		return 0;                                // frequency out of range
	}
	last_ticks = ticks;
	set_timing(ts);

	start_idx = 0;
	store_idx = 0;
	n         = 0;
	wp        = &adc_buf[0][0];
	done      = 0;
	running   = 1;                               // from here the edge ISR starts captures
	while (!done) ;
	return 1;
}

uint16_t adc_period_ticks(void)
{
	return last_ticks;
}