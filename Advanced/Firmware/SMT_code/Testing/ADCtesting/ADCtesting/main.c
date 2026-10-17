#include <avr/io.h>
#include <avr/interrupt.h>
#include "adc.h"
#include "uart.h"          // defines F_CPU, must come before util/delay.h
#include <util/delay.h>

#define CYCLE_DELAY_MS  1000   // pause after each full capture + transmit

// print the value, or nothing at all if the slot wasn't captured
static void put_sample(uint16_t v)
{
	if (v != ADC_EMPTY) {
		uart_put_u16(v);
	}
}

int main(void)
{
	uart_init();
	adc_init();
	sei();

	while (1) {
		adc_capture();                       // fills 160 points per signal

		// one line per instant: voltage,current  (CR+LF), blank line after the set
		// an uncaptured slot is left empty, e.g. ",123" or "123,"
		for (uint8_t i = 0; i < ADC_POINTS; i++) {
			put_sample(adc_buf[ADC_VOLT][i]);
			uart_putc(',');
			put_sample(adc_buf[ADC_CURR][i]);
			uart_puts("\r\n");
		}
		uart_puts("\r\n");

		_delay_ms(CYCLE_DELAY_MS);
	}
}