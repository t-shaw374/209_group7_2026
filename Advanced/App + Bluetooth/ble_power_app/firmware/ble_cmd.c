/*
 * ble_cmd.c  -  see ble_cmd.h
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "ble_cmd.h"

/* ATmega328PB has two USARTs and names this vector USART0_RX_vect; ATmega328P uses USART_RX_vect. */
#if defined(USART0_RX_vect)
#define BLE_RX_VECT USART0_RX_vect
#else
#define BLE_RX_VECT USART_RX_vect
#endif

#define RB_SIZE 64                       /* power of two */

static volatile uint8_t rb[RB_SIZE];
static volatile uint8_t rb_head = 0;     /* written by the ISR */
static volatile uint8_t rb_tail = 0;     /* read by main */

static char    line[BLE_CMD_MAX];
static uint8_t len = 0;

ISR(BLE_RX_VECT)
{
	uint8_t c = UDR0;                    /* always read UDR0 to clear the interrupt */
	uint8_t next = (uint8_t)((rb_head + 1) & (RB_SIZE - 1));
	if (next != rb_tail) {               /* full: drop the byte */
		rb[rb_head] = c;
		rb_head = next;
	}
}

void ble_cmd_init(void)
{
	UCSR0B |= (1 << RXEN0) | (1 << RXCIE0);   /* receiver + RX-complete interrupt */
}

uint8_t ble_cmd_get(char out[BLE_CMD_MAX])
{
	while (rb_tail != rb_head) {
		char c = (char)rb[rb_tail];
		rb_tail = (uint8_t)((rb_tail + 1) & (RB_SIZE - 1));

		if (c == '\r') continue;
		if (c == '\n') {
			for (uint8_t i = 0; i < len; i++) out[i] = line[i];
			out[len] = '\0';
			len = 0;
			return 1;
		}
		if (len < BLE_CMD_MAX - 1) line[len++] = c;
	}
	return 0;
}
