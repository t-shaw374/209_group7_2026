/*
 * UART.h
 *
 * Created: 15/09/2026 4:27:11 PM
 *  Author: TAYE752
 */ 


#ifndef _UART_H
#define _UART_H

#include <stdint.h>

void usart_init(uint16_t ubrr);
void usart_transmit(uint8_t data);
void usart_send_number(uint16_t value);

#endif