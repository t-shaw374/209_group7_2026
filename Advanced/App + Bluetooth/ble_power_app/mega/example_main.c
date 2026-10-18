/*
 * example_main.c  -  how to hook ble_report into the energy-monitor firmware.
 *
 * Replace the "compute" section with your real Vrms / Irms / P / PF maths
 * (from the ADC samples on PC0 / PC1 and the offset on PC2). The values below
 * are placeholders so you can test the BLE link and app before the maths is done.
 */

#include "common.h"          /* F_CPU 2000000UL, UBRR 12 (9600 baud) */
#include <avr/io.h>
#include <util/delay.h>
#include "UART.h"
#include "ble_report.h"

int main(void)
{
	usart_init(UBRR);

	uint16_t t = 0;

	while (1) {
		/* ---- compute (placeholder values: replace with real calculations) ---- */
		uint32_t vrms_mV  = 12980;                 /* 12.98 V */
		uint32_t irms_mA  = 263;                   /* 0.263 A */
		uint32_t p_mW     = 3000 + (t % 10) * 100; /* ramps 3.00 -> 3.90 W */
		uint16_t pf_milli = 990;                   /* 0.99   */
		/* ---------------------------------------------------------------------- */

		if (t % 10 == 0) {
			ble_send_axis(0, 8000);   /* graph 0-8 W: rated max 7.5 VA */
		}
		ble_send_reading(t, vrms_mV, irms_mA, p_mW, pf_milli);

		t++;
		_delay_ms(1000);
	}
}
