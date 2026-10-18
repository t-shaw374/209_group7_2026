#include <avr/io.h>
#include "uart.h"

#define UBRR_VAL  ((F_CPU + 8UL * UART_BAUD) / (16UL * UART_BAUD) - 1)

void uart_init(void)
{
	UBRR0H = (uint8_t)(UBRR_VAL >> 8);
	UBRR0L = (uint8_t)UBRR_VAL;
	UCSR0B = (1 << TXEN0);                      // transmitter only
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);     // 8 data bits, no parity, 1 stop
}

void uart_putc(char c)
{
	while (!(UCSR0A & (1 << UDRE0))) ;          // wait for empty data register
	UDR0 = c;
}

void uart_puts(const char *s)
{
	while (*s) {
		uart_putc(*s++);
	}
}

void uart_put_u16(uint16_t v)
{
	char tmp[5];
	uint8_t i = 0;

	do {
		tmp[i++] = '0' + (v % 10);
		v /= 10;
	} while (v);

	while (i) {
		uart_putc(tmp[--i]);
	}
}