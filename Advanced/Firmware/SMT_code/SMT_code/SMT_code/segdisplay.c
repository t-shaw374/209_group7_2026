/*
 * _7seg.c
 *
 * Created: 16/10/2026 8:49:28 pm
 *  Author: thoma
 */ 
#include "common.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include "segdisplay.h"

// bit order: dp g f e d c b a
static const uint8_t seg_pattern[10] PROGMEM = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

static volatile uint8_t disp_characters[4] = {0,0,0,0};
static volatile uint8_t disp_position = 0;

// ---- Pin definitions (from schematic) ----
// Shift register on PORTB
#define SH_CP_pin 0
#define SH_ST_pin 1
#define SH_DS_pin 2
// Digit selects on PORTD (active low)
#define DS1_pin   7
#define DS2_pin   6
#define DS3_pin   5
#define DS4_pin   4
// Shift register output enable (assumed active low)
#define OE_pin    3

#define DS_MASK ((1 << DS1_pin) | (1 << DS2_pin) | (1 << DS3_pin) | (1 << DS4_pin))

void init_display(void)
{
	DDRB |= (1 << SH_CP_pin) | (1 << SH_DS_pin) | (1 << SH_ST_pin);
	DDRD |= DS_MASK | (1 << OE_pin);

	PORTB &= ~((1 << SH_CP_pin) | (1 << SH_ST_pin));
	PORTD |= DS_MASK;           // all digits off
	PORTD &= ~(1 << OE_pin);    // enable shift register outputs
}

void seperate_and_load_characters(uint16_t number, uint8_t decimal_pos)
{
	uint8_t c[4];
	c[0] = pgm_read_byte(&seg_pattern[(number / 1000) % 10]);
	c[1] = pgm_read_byte(&seg_pattern[(number / 100) % 10]);
	c[2] = pgm_read_byte(&seg_pattern[(number / 10) % 10]);
	c[3] = pgm_read_byte(&seg_pattern[number % 10]);

	if (decimal_pos < 4) c[decimal_pos] |= 0x80;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		for (uint8_t i = 0; i < 4; i++) disp_characters[i] = c[i];
	}
}

void send_next_character_to_display(void)
{
	uint8_t pattern = disp_characters[disp_position];

	PORTB &= ~(1 << SH_CP_pin);
	for (int8_t i = 7; i >= 0; i--)
	{
		if (pattern & (1 << i)) PORTB |= (1 << SH_DS_pin);
		else                    PORTB &= ~(1 << SH_DS_pin);

		PORTB |= (1 << SH_CP_pin);
		PORTB &= ~(1 << SH_CP_pin);
	}

	PORTD |= DS_MASK;                 // digits off before latching
	PORTB |= (1 << SH_ST_pin);        // latch
	PORTB &= ~(1 << SH_ST_pin);

	switch (disp_position)
	{
		case 0: PORTD &= ~(1 << DS1_pin); break;
		case 1: PORTD &= ~(1 << DS2_pin); break;
		case 2: PORTD &= ~(1 << DS3_pin); break;
		case 3: PORTD &= ~(1 << DS4_pin); break;
	}

	if (++disp_position > 3) disp_position = 0;
}