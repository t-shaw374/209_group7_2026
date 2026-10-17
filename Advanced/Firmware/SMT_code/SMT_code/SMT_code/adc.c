/*
 * adc.c
 *
 * Created: 14/10/2026 12:43:33 pm
 *  Author: tsha374
 *
 * Interleaved (equivalent-time) sampling of a 2 ms period signal.
 *
 * - Each pass takes 16 samples, 125 us apart, starting at a zero crossing.
 * - Each pass is shifted 12.5 us later than the previous one (10 passes).
 * - Voltage (ADC1) and current (ADC2) alternate on successive zero
 *   crossings; a voltage pass and the current pass after it share an offset.
 * - Result: 160 points per channel at 12.5 us spacing, index = sample*10 + pass.
 * - The 7-seg scan is paused for the whole capture to avoid timing jitter.
 */
#include "adc.h"
#include "segdisplay.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#define T1_PERIOD_TICKS    4000U                          // 2 ms at 2 MHz
#define T1_STEP_TICKS      250U                           // 125 us between samples in a pass
#define NUM_CYCLES         10                             // passes (offsets) per channel
#define OFFSET_TICKS       (T1_STEP_TICKS / NUM_CYCLES)   // 25 ticks = 12.5 us per pass
#define SAMPLES_PER_PASS   16
#define NUM_POINTS         (SAMPLES_PER_PASS * NUM_CYCLES) // 160

#define CH_VOLTAGE_ADC     1      // ADC1 / PC1
#define CH_CURRENT_ADC     2      // ADC2 / PC2

// Raw values: row 0 is voltage, row 1 is current. Private to this file.
static volatile uint16_t raw_array[2][NUM_POINTS];
static volatile uint8_t array_pos       = 0;   // current write position
static volatile uint8_t signal_select   = 1;   // 0 = voltage, 1 = current
static volatile uint8_t num_adc_samples = 0;   // samples taken in current pass
static volatile uint8_t match_count     = 0;   // compare-B matches (triggers) in current pass
static volatile uint8_t cycle_num       = NUM_CYCLES - 1;  // current offset index
static volatile uint8_t capture_busy    = 0;   // a pass is currently sampling
static volatile uint8_t capture_running = 0;   // a full capture is in progress
static volatile uint8_t capture_finished = 0;  // all passes collected

// ---------------------------------------------------------------------
// Trigger and channel helpers
// ---------------------------------------------------------------------
static void adc_trigger_enable(void)
{
	TIFR1   = (1 << OCF1B);          // clear any stale trigger flag first
	TIMSK1 |= (1 << OCIE1B);         // compare-B ISR re-arms OCR1B (also clears the flag)
	ADCSRA |= (1 << ADATE);          // auto-trigger on Timer1 compare B
}

static void adc_trigger_disable(void)
{
	ADCSRA  &= ~(1 << ADATE);        // ADC ignores the timer
	TIMSK1  &= ~(1 << OCIE1B);       // no more re-arming
}

static void adc_switch_channel(uint8_t channel)
{
	// Keep reference bits, replace channel bits (MUX3:0)
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
}

// ---------------------------------------------------------------------
// Zero crossing on PC0 (PCINT8)
// ---------------------------------------------------------------------
void zc_init(void)
{
	DDRC   &= ~(1 << DDC0);       // PC0 as input (add a pull-up only if your circuit needs it)
	PCMSK1 |=  (1 << PCINT8);     // allow PC0 to trigger the group 1 vector
	PCICR  |=  (1 << PCIE1);      // enable pin-change interrupt group 1 (PC0..PC6)
}

ISR(PCINT1_vect)
{
	if (!(PINC & (1 << PINC0))) return;   // pin-change fires on both edges: rising only
	if (!capture_running) return;         // nobody asked for a capture
	if (capture_busy) return;             // previous pass still sampling, skip this crossing

	signal_select = !signal_select;
	adc_switch_channel(signal_select ? CH_CURRENT_ADC : CH_VOLTAGE_ADC);

	if (signal_select == 0) {             // voltage starts a new pair = new offset
		cycle_num = (cycle_num + 1) % NUM_CYCLES;
	}

	array_pos       = cycle_num;          // current reuses its voltage's offset
	num_adc_samples = 0;
	match_count     = 0;
	capture_busy    = 1;

	TCNT1 = 0;                            // align timer to the zero crossing
	OCR1B = cycle_num * OFFSET_TICKS + 1; // first sample; +1 because writing TCNT1 blocks a match at 0
	adc_trigger_enable();
}

// ---------------------------------------------------------------------
// Timer1 compare B: fires at every sample trigger
//
// The match sets OCF1B (which starts the conversion); running this ISR
// clears it again so the next match makes a fresh rising edge. The next
// match is scheduled here, right after the current one, so there is a
// full 125 us of margin. Doing it at the end of the ADC ISR left only
// ~30 ticks after the conversion finished, which the ISR could overrun.
// ---------------------------------------------------------------------
ISR(TIMER1_COMPB_vect)
{
	if (++match_count < SAMPLES_PER_PASS) {
		OCR1B += T1_STEP_TICKS;           // next sample 125 us later (max 3976, never wraps)
	} else {
		OCR1B = T1_PERIOD_TICKS;          // above OCR1A, so no 17th trigger can happen
	}
}

// ---------------------------------------------------------------------
// ADC conversion complete
// ---------------------------------------------------------------------
ISR(ADC_vect)
{
	if (!capture_running) return;         // capture was aborted while a conversion was in flight

	raw_array[signal_select][array_pos] = ADC;
	array_pos += NUM_CYCLES;              // stride 10: index = sample*10 + pass, max 159

	if (++num_adc_samples >= SAMPLES_PER_PASS) {
		adc_trigger_disable();
		capture_busy = 0;

		// Last pass of the last pair (current channel, final offset): done
		if (signal_select == 1 && cycle_num == NUM_CYCLES - 1) {
			capture_running  = 0;
			capture_finished = 1;
			display_resume();             // 7-seg scan back on
		}
	}
}

// ---------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------
void adc_timer_init(void)
{
	// Timer1: CTC, no prescaler, counts 0..OCR1A then restarts
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS10);
	OCR1A  = T1_PERIOD_TICKS - 1;
	OCR1B  = T1_STEP_TICKS;

	// ADC: auto-trigger source is Timer1 compare match B, but ADATE stays
	// off until a zero crossing starts a pass
	ADMUX  = CH_VOLTAGE_ADC;                 // external AREF
	DIDR0 |= (1 << ADC1D) | (1 << ADC2D);    // digital buffers off on PC1 and PC2
	ADCSRB = (1 << ADTS2) | (1 << ADTS0);    // 101 = Timer1 compare match B
	ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2);
}

// Begin a full capture: 10 voltage/current pairs. Pauses the 7-seg scan
// until the capture finishes.
void adc_start_capture(void)
{
	display_pause();

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		signal_select    = 1;                 // first crossing flips to voltage
		cycle_num        = NUM_CYCLES - 1;    // first pair wraps to offset 0
		num_adc_samples  = 0;
		capture_busy     = 0;
		capture_finished = 0;
		capture_running  = 1;
	}
}

// 1 once all passes have been collected
uint8_t adc_capture_done(void)
{
	return capture_finished;
}

// Read one collected sample (channel 0 = voltage, 1 = current, index 0..159).
// Only call once adc_capture_done() returns 1.
uint16_t adc_get_raw(uint8_t channel, uint8_t index)
{
	return raw_array[channel][index];
}

// Give up on a capture (e.g. no zero crossing seen): stop sampling and
// bring the 7-seg scan back.
void adc_abort_capture(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		capture_running = 0;
		capture_busy    = 0;
	}
	adc_trigger_disable();
	display_resume();
}

uint16_t adc_to_mv(uint16_t raw)
{
	// Rounded: mV = raw * 3300 / 1024
	uint16_t voltage_mV = (uint16_t)(((uint32_t)raw * ADC_VREF_MV + (ADC_STEPS / 2)) / ADC_STEPS);
	return voltage_mV;
}

void adc_mv_to_str(uint16_t mv, char *buf)
{
	if (mv > 9999) mv = 9999;                   // clamp to 4 digits

	// Fill digits from least significant to most significant
	buf[3] = '0' + (mv % 10); mv /= 10;         // ones
	buf[2] = '0' + (mv % 10); mv /= 10;         // tens
	buf[1] = '0' + (mv % 10); mv /= 10;         // hundreds
	buf[0] = '0' + mv;                          // thousands
	buf[4] = '\0';                              // null terminator
}