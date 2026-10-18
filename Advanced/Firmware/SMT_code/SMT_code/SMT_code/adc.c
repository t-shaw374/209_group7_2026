// Equivalent-time sampling (ETS) of voltage and current, ATmega328PB @ 2 MHz
//
// - ADC clock = 2 MHz / 16 = 125 kHz  (conversion = 13 ADC clocks = 104 us)
// - Timer1 CTC (mode 4, TOP = OCR1A), compare match B every 250 ticks (125 us)
//   auto-triggers the ADC. Timer0 is not used here.
// - Zero-crossing signal on PC0 (ADC0 pin, digital input) via pin-change interrupt.
// - One capture = exactly 16 samples = one 2 ms period. A rising edge starts the
//   timer, and the timer stops itself after the 16th trigger. The next capture
//   starts on the very next rising edge, so a capture takes one period. For the
//   last offsets (about 7..9) the 16th trigger lands too close to the next edge,
//   so that edge is skipped and those captures take two periods.
// - Each offset index delays the first sample by another 25 ticks (12.5 us), and
//   samples are stored at index n * 10 + offset, so 10 offsets interleave into
//   one finer-grained period of 160 points.
// - Capture order: V offset 0, I offset 0, V offset 1, I offset 1 ... Both signals
//   at one offset start from a zero-crossing edge, so index i is the same instant
//   in both buffers.
// - The channel is switched in the edge interrupt, just before the timer starts.
//   The ADC locks its channel once a conversion has started, so switching while
//   the last conversion of the previous capture is still running is safe.
// - Timer1 is 16-bit, so T_SAMPLE can go above 255 ticks if the signal period
//   gets longer. Keep it at least ~210 ticks (104 us conversion plus margin).
//
// Signals to be measured: ADC1 (PC1, voltage) and ADC2 (PC2, current).

#include <avr/io.h>
#include <avr/interrupt.h>
#include "adc.h"

#define T_SAMPLE  250    // timer ticks between samples (125 us @ 2 MHz)
#define STEP      25     // timer ticks per offset step (12.5 us)
#define T_FIRST   8      // timer ticks from start to the first trigger at offset 0
#define CAPTURES  (2 * ADC_OFFSETS)

// WGM12 (CTC mode bit) lives in TCCR1B together with the clock select bits,
// so stopping the timer must keep WGM12 set. Never write TCCR1B = 0.
#define T1_STOPPED  (1 << WGM12)                 // CTC mode 4, no clock
#define T1_RUNNING  ((1 << WGM12) | (1 << CS10)) // CTC mode 4, prescaler 1

volatile uint16_t adc_buf[2][ADC_POINTS];

static const uint8_t mux_ch[2] = { ADC_CH_V, ADC_CH_I };

static volatile uint8_t  start_idx;    // captures started (edge ISR)
static volatile uint8_t  trig;         // triggers so far in the current capture (compare ISR)
static volatile uint8_t  store_idx;    // capture whose samples are being stored (ADC ISR)
static volatile uint8_t  n;            // samples stored so far in that capture (ADC ISR)
static volatile uint16_t * volatile wp;// where the next result goes (ADC ISR)
static volatile uint8_t  running;      // a capture sequence is in progress
static volatile uint8_t  done;         // all captures stored

// Zero crossing on PC0, rising edge only. Starts the next capture, unless all are
// started, or the timer is still running. The timer is still running when the 16th
// trigger of the previous capture comes too close to this edge (offsets ~7..9, or
// a period shorter than 2 ms). That edge is skipped and the capture starts on the
// next one, so those captures take two periods. Nothing is corrupted.
// (This ISR needs ~36 us from the edge to starting the timer. That delay is the
// same every time, so it only shifts all samples by a constant.)
// TCNT1 is a 16-bit write; it is safe here because this ISR runs with interrupts
// off and no other ISR touches 16-bit Timer1 registers.
ISR(PCINT1_vect)
{
	if (!(PINC & (1 << PC0))) {
		return;                                      // falling edge, ignore
	}
	if (running && start_idx < CAPTURES && TCCR1B == T1_STOPPED) {
		uint8_t i = start_idx++;
		ADMUX  = (ADMUX & 0xF0) | mux_ch[i & 1];     // V on even, I on odd captures
		TCNT1  = (T_SAMPLE - T_FIRST) - STEP * (i >> 1);
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
	// (200 us) instead of 13, which is longer than the 125 us trigger spacing
	// and makes the second trigger get lost. Get it out of the way here.
	ADCSRA = (1 << ADEN) | (1 << ADPS2);          // enable, prescaler 16
	ADCSRA |= (1 << ADSC);                        // start one conversion
	while (ADCSRA & (1 << ADSC)) ;                // wait for it to finish
	ADCSRA |= (1 << ADIF);                        // clear the done flag (write 1)

	// Now switch on auto-trigger and the conversion-complete interrupt
	ADCSRA = (1 << ADEN) | (1 << ADATE) | (1 << ADIE) | (1 << ADPS2);

	// Timer1: CTC mode 4, TOP = OCR1A, started later by the zero-crossing ISR.
	// OCR1B = TOP, so match B lands at the same count the old Timer0 match did.
	TCCR1A = 0;
	TCCR1B = T1_STOPPED;
	OCR1A  = T_SAMPLE - 1;
	OCR1B  = T_SAMPLE - 1;
	TIMSK1 = (1 << OCIE1B);

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

void adc_capture(void)
{
	start_idx = 0;
	store_idx = 0;
	n         = 0;
	wp        = &adc_buf[0][0];
	done      = 0;
	running   = 1;                           // from here the edge ISR starts captures
	while (!done) ;
}