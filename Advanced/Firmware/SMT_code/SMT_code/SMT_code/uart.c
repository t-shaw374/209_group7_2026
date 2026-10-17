/*
 * uart.c
 *
 * Created: 17/10/2026 8:11:29 pm
 *  Author: tsha374
 */ 

#include <avr/io.h>
#include "uart.h"

void usart_init(uint16_t ubrr) {
	UCSR0B = (1 << TXEN0);
	UCSR0C = (1 << UCSZ01 | 1 << UCSZ00);
	UBRR0L = (uint8_t)(ubrr);
	UBRR0H = (uint8_t)(ubrr >> 8);
}

void usart_transmit(uint8_t data) {
	while (!(UCSR0A & (1 << UDRE0))) {}
	UDR0 = data;
	
}

void uart_tx_string(const char *s) {
	while (*s) {
		usart_transmit(*s++);
	}
}

void send_mV_value(uint16_t num) {
	uint8_t num_chars[] = {(num / 1000) % 10, (num / 100) % 10, (num / 10) % 10, num % 10};
	for(uint8_t i = 0; i < 4; i++) {
		usart_transmit(num_chars[i] + 48);
	}
}