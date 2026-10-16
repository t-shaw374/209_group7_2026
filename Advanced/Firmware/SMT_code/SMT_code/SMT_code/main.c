/*
 * SMT_code.c
 *
 * Created: 14/10/2026 12:20:34 pm
 * Author : tsha374
 */ 
#include "common.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "i2c.h"
#include "sh1106.h"
#include "graph.h"
#include "adc.h"
#include "segdisplay.h"

static void timer0_init(void)
{
	TCCR0A = (1 << WGM01);      // CTC mode
	TCCR0B = (1 << CS02);       // prescaler 256 -> 7812.5 Hz
	OCR0A  = 7;                 // 8 counts -> ~977 Hz, ~244 Hz full refresh
	TIMSK0 = (1 << OCIE0A);
}

ISR(TIMER0_COMPA_vect)
{
	send_next_character_to_display();
}

int main(void)
{
	char mv_str[ADC_STR_LEN];

	init_display();
	i2c_init();
	sh1106_init();
	adc_init();
	timer0_init();
	sei();

	while (1) {
		uint16_t raw = adc_read(1);
		uint16_t mv  = adc_to_mv(raw);

		seperate_and_load_characters(mv, 0);   // 7-seg: e.g. 3.300
		adc_mv_to_str(mv, mv_str);
		graph_render(mv_str);                  // OLED

		_delay_ms(1000);
	}
}

