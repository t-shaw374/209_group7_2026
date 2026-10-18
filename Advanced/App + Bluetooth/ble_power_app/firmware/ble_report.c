/*
 * ble_report.c  -  see ble_report.h for the line format
 */

#include "ble_report.h"
#include "UART.h"
#include "pgm_compat.h"

void ble_send_str(const char *s)
{
	while (*s) {
		usart_transmit((uint8_t)*s++);
	}
}

void ble_send_P(const char *p)
{
	char c;
	while ((c = (char)pgm_read_byte(p++))) {
		usart_transmit((uint8_t)c);
	}
}

void ble_send_uint(uint32_t v)
{
	char digits[10];
	uint8_t n = 0;
	do {
		digits[n++] = (char)('0' + (v % 10));
		v /= 10;
	} while (v > 0);
	while (n > 0) {
		usart_transmit((uint8_t)digits[--n]);
	}
}

void ble_send_fixed(uint32_t milli, uint8_t decimals)
{
	uint32_t div;   /* milli-units per output step: 100, 10 or 1 */
	uint32_t scale; /* 10^decimals */

	if (decimals == 1)      { div = 100; scale = 10;   }
	else if (decimals == 2) { div = 10;  scale = 100;  }
	else                    { div = 1;   scale = 1000; }

	uint32_t v    = (milli + div / 2) / div;   /* rounded */
	uint32_t frac = v % scale;

	ble_send_uint(v / scale);
	usart_transmit('.');
	for (uint32_t p = scale / 10; p > 0; p /= 10) {   /* zero-pad: 0.05 not 0.5 */
		usart_transmit((uint8_t)('0' + (frac / p) % 10));
	}
}

void ble_send_axis(uint32_t p_min_mW, uint32_t p_max_mW)
{
	ble_send_P(PSTR("A,"));
	ble_send_fixed(p_min_mW, 2);
	usart_transmit(',');
	ble_send_fixed(p_max_mW, 2);
	ble_send_P(PSTR(",W\n"));
}

void ble_send_reading(uint32_t t_ms, uint32_t vrms_mV, uint32_t irms_mA,
                      uint32_t p_mW, uint16_t pf_milli, uint32_t e_mWh)
{
	ble_send_P(PSTR("D,"));
	ble_send_fixed(t_ms, 1);
	usart_transmit(',');
	ble_send_fixed(vrms_mV, 2);
	usart_transmit(',');
	ble_send_fixed(irms_mA, 3);
	usart_transmit(',');
	ble_send_fixed(p_mW, 2);
	usart_transmit(',');
	ble_send_fixed(pf_milli, 2);
	usart_transmit(',');
	ble_send_fixed(e_mWh, 3);
	usart_transmit('\n');
}

void ble_send_minmax(uint32_t p_min_mW, uint32_t p_max_mW)
{
	ble_send_P(PSTR("M,"));
	ble_send_fixed(p_min_mW, 2);
	usart_transmit(',');
	ble_send_fixed(p_max_mW, 2);
	usart_transmit('\n');
}
