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

static volatile char    line[BLE_CMD_MAX];
static volatile uint8_t len   = 0;
static volatile uint8_t ready = 0;

ISR(BLE_RX_VECT)
{
	char c = UDR0;                 /* always read UDR0 to clear the interrupt */
	if (ready) return;             /* previous line not collected yet: drop */
	if (c == '\r') return;
	if (c == '\n') {
		line[len] = '\0';
		len = 0;
		ready = 1;
		return;
	}
	if (len < BLE_CMD_MAX - 1) line[len++] = c;
}

void ble_cmd_init(void)
{
	UCSR0B |= (1 << RXEN0) | (1 << RXCIE0);   /* receiver + RX-complete interrupt */
}

uint8_t ble_cmd_get(char out[BLE_CMD_MAX])
{
	if (!ready) return 0;
	for (uint8_t i = 0; i < BLE_CMD_MAX; i++) {
		out[i] = line[i];
		if (line[i] == '\0') break;
	}
	ready = 0;
	return 1;
}
