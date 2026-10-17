/*
 * uart.h
 *
 * Created: 17/10/2026 8:11:40 pm
 *  Author: tsha374
 */ 
#include <stdint.h>

#ifndef UART_H_
#define UART_H_

void usart_init(uint16_t ubrr);
void usart_transmit(uint8_t data);
void send_mV_value(uint16_t num);
void uart_tx_string(const char *s);



#endif /* UART_H_ */