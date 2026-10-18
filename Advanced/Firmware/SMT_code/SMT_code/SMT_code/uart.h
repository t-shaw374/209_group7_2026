#ifndef UART_H
#define UART_H

#include <stdint.h>

#ifndef F_CPU
#define F_CPU 2000000UL
#endif

#define UART_BAUD 9600UL     // 2 MHz: UBRR = 12, 0.16 % error

void uart_init(void);                 // TX only, 8N1
void uart_putc(char c);               // blocking
void uart_puts(const char *s);        // blocking, null-terminated string
void uart_put_u16(uint16_t v);        // value as decimal text (no newline)

#endif