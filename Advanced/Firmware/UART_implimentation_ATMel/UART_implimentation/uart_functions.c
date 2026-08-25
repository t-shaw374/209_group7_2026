/*
 * uart_functions.c
 *
 * Created: 25/08/2026 4:01:30 pm
 *  Author: thoma
 */ 

#include <stdint.h>
#include <avr/io.h>
#include "uart_header.h"

void usart_init(uint16_t ubrr)
{
	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)(ubrr);
	UCSR0B = (1 << TXEN0);                       // transmitter on
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);      // 8-N-1
}

void usart_transmit(uint8_t data)
{
	while (!(UCSR0A & (1 << UDRE0)))
	{
		;
	}
	UDR0 = data;
}

void usart_print_string(const char *s)
{
	while (*s)
	{
		usart_transmit((uint8_t)*s);
		s++;
	}
}