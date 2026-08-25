/*
 * uart_header.h
 *
 * Created: 25/08/2026 4:01:15 pm
 *  Author: thoma
 */ 


#ifndef UART_HEADER_H_
#define UART_HEADER_H_

void usart_init(uint16_t ubrr);
void usart_transmit(uint8_t data);
void usart_print_string(const char *s);




#endif /* UART_HEADER_H_ */