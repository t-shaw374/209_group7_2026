/*
 * UART.c
 *
 * Created: 15/09/2026 4:26:25 PM
 *  Author: TAYE752
 */ 

#include "common.h"
#include "UART.h"
#include <avr/io.h>

void usart_init(uint16_t ubrr) {
	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)(ubrr);
	UCSR0B = (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void usart_transmit(uint8_t data) {
	while (!(UCSR0A & (1 << UDRE0))) { ; }
	UDR0 = data;
}

void usart_send_number(uint16_t value) {
	uint8_t thousands = (value / 1000) % 10;
	uint8_t hundreds  = (value / 100)  % 10;
	uint8_t tens      = (value / 10)   % 10;
	uint8_t ones      =  value         % 10;
	usart_transmit(thousands + '0');
	usart_transmit(hundreds  + '0');
	usart_transmit(tens      + '0');
	usart_transmit(ones      + '0');
}