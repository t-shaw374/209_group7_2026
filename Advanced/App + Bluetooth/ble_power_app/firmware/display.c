/*
 * display.c
 *
 * Created: 29/09/2026 2:42:21 pm
 *  Author: tsha374   (Lab 6 Part 2 driver)
 * Extended for the energy monitor: unit letters, value formatter, system tick.
 */

#ifndef DISPLAY_HOST_TEST
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#endif
#include "display.h"

// Array containing which segments to turn on to display a number between 0 to 9
// bit order: dp g f e d c b a
static const uint8_t seg_pattern[10] = {
	0x3F, // 0
	0x06, // 1
	0x5B, // 2
	0x4F, // 3
	0x66, // 4
	0x6D, // 5
	0x7D, // 6
	0x07, // 7
	0x7F, // 8
	0x6F  // 9
};

// 4 characters to be displayed on Ds1 to Ds4
static volatile uint8_t disp_characters[4] = {0,0,0,0};

void display_format(uint32_t milli, uint8_t unit_pattern, uint8_t out[4])
{
	uint16_t c;     /* 3-digit number to show */
	uint8_t  dp;    /* digit that carries the decimal point */

	if (milli < 9995UL)        { c = (uint16_t)((milli + 5) / 10);     dp = 0; } /* x.yy */
	else if (milli < 99950UL)  { c = (uint16_t)((milli + 50) / 100);   dp = 1; } /* xx.y */
	else if (milli < 999500UL) { c = (uint16_t)((milli + 500) / 1000); dp = 3; } /* xxx  */
	else {
		out[0] = out[1] = out[2] = SEG_DASH;
		out[3] = unit_pattern;
		return;
	}

	out[0] = seg_pattern[(c / 100) % 10];
	out[1] = seg_pattern[(c / 10) % 10];
	out[2] = seg_pattern[c % 10];
	out[3] = unit_pattern;
	if (dp < 3) out[dp] |= 0x80;
}

#ifndef DISPLAY_HOST_TEST

// The current digit (e.g. the 1's, the 10's) of the 4-digit number we're displaying
static volatile uint8_t disp_position = 0;

static volatile uint32_t tick_count = 0;

// ---- Pin definitions (SMT board, header P6) ----
// shift register on PORTB, digit enables + output enable on PORTD
#define SH_CP_pin 0     // PB0
#define SH_ST_pin 1     // PB1
#define SH_DS_pin 2     // PB2
#define DS1_pin   7     // PD7
#define DS2_pin   6     // PD6
#define DS3_pin   5     // PD5
#define DS4_pin   4     // PD4
#define OE_pin    3     // PD3: shift register output enable, active low

void init_display(void)
{
	// SH_DS, SH_CP, SH_ST on PORTB as outputs
	DDRB |= (1 << SH_CP_pin) | (1 << SH_DS_pin) | (1 << SH_ST_pin);

	// Ds1-Ds4 and OE on PORTD as outputs
	DDRD |= (1 << DS1_pin) | (1 << DS2_pin) | (1 << DS3_pin) | (1 << DS4_pin) | (1 << OE_pin);

	// Shift register control lines start low
	PORTB &= ~((1 << SH_CP_pin) | (1 << SH_ST_pin));

	// All digits disabled initially (logic 1 = off)
	PORTD |= (1 << DS1_pin) | (1 << DS2_pin) | (1 << DS3_pin) | (1 << DS4_pin);

	// Shift register outputs on (OE is active low)
	PORTD &= ~(1 << OE_pin);
}

// Populate the array 'disp_characters[]' by separating the four digits of 'number'
// and then looking up the segment pattern from 'seg_pattern[]'
void seperate_and_load_characters(uint16_t number, uint8_t decimal_pos)
{
	uint8_t ones, tens, hundreds, thousands;

	ones      = number % 10;
	tens      = (number / 10) % 10;
	hundreds  = (number / 100) % 10;
	thousands = (number / 1000) % 10;

	uint8_t p[4] = { seg_pattern[thousands], seg_pattern[hundreds],
	                 seg_pattern[tens], seg_pattern[ones] };
	if (decimal_pos < 4) p[decimal_pos] |= 0x80;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		for (uint8_t k = 0; k < 4; k++) disp_characters[k] = p[k];
	}
}

void display_value(uint32_t milli, uint8_t unit_pattern)
{
	uint8_t p[4];
	display_format(milli, unit_pattern, p);
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		for (uint8_t k = 0; k < 4; k++) disp_characters[k] = p[k];
	}
}

void display_raw(const uint8_t p[4])
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		for (uint8_t k = 0; k < 4; k++) disp_characters[k] = p[k];
	}
}

void send_next_character_to_display(void)
{
	// 1. Load the digit to send
	uint8_t pattern = disp_characters[disp_position];

	// 2. Send this bit pattern to the shift register
	PORTB &= ~(1 << SH_CP_pin);

	for (int8_t i = 7; i >= 0; i--)
	{
		if (pattern & (1 << i)) {
			PORTB |= (1 << SH_DS_pin);
		} else {
			PORTB &= ~(1 << SH_DS_pin);
		}
		PORTB |= (1 << SH_CP_pin);
		PORTB &= ~(1 << SH_CP_pin);
	}

	// 3. Disable all digits BEFORE latching the new pattern
	PORTD |= (1 << DS1_pin) | (1 << DS2_pin) | (1 << DS3_pin) | (1 << DS4_pin);

	// 4. Latch the output by toggling SH_ST
	PORTB |= (1 << SH_ST_pin);
	PORTB &= ~(1 << SH_ST_pin);

	// 5. Enable the correct digit
	switch (disp_position)
	{
		case 0: PORTD &= ~(1 << DS1_pin); break;
		case 1: PORTD &= ~(1 << DS2_pin); break;
		case 2: PORTD &= ~(1 << DS3_pin); break;
		case 3: PORTD &= ~(1 << DS4_pin); break;
	}

	// 6. Increment disp_position, wrap after 3
	disp_position++;
	if (disp_position > 3)
	{
		disp_position = 0;
	}
}

void display_timer_init(void)
{
	TCCR0A = (1 << WGM01);                 // CTC mode
	TCCR0B = (1 << CS01) | (1 << CS00);    // prescaler 64 -> 31.25 kHz
	OCR0A  = 77;                           // 78 counts = 2.5 ms per digit, 100 Hz refresh
	TIMSK0 = (1 << OCIE0A);
}

/* ISR_NOBLOCK: interrupts are re-enabled on entry so the ADC interrupt is never
 * held up by the ~70 us it takes to shift a digit out at 2 MHz. */
ISR(TIMER0_COMPA_vect, ISR_NOBLOCK)
{
	tick_count++;
	send_next_character_to_display();
}

uint32_t ticks(void)
{
	uint32_t t;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { t = tick_count; }
	return t;
}

#endif /* DISPLAY_HOST_TEST */
