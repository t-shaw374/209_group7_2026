/*
 * example_main.c  -  how to hook ble_report / ble_cmd into the energy-monitor firmware.
 *
 * Replace the "compute" section with your real Vrms / Irms / P / PF maths
 * (from the ADC samples on PC0 / PC1 and the offset on PC2). The values below
 * are placeholders so you can test the BLE link and app before the maths is done.
 */

#include "common.h"          /* F_CPU 2000000UL, UBRR 12 (9600 baud) */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include "UART.h"
#include "ble_report.h"
#include "ble_cmd.h"

/* Readings below this count as "no load" and are left out of min/max,
 * otherwise the minimum would just read 0.00 W whenever nothing is plugged in. */
#define NO_LOAD_mW  50

int main(void)
{
	usart_init(UBRR);
	ble_cmd_init();
	sei();

	uint16_t t = 0;
	uint32_t p_min_mW = 0, p_max_mW = 0;
	uint8_t  have_minmax = 0;
	char     cmd[BLE_CMD_MAX];

	while (1) {
		/* ---- compute (placeholder values: replace with real calculations) ---- */
		uint32_t vrms_mV  = 12980;                 /* 12.98 V */
		uint32_t irms_mA  = 263;                   /* 0.263 A */
		uint32_t p_mW     = 3000 + (t % 10) * 100; /* ramps 3.00 -> 3.90 W */
		uint16_t pf_milli = 990;                   /* 0.99   */
		/* ---------------------------------------------------------------------- */

		/* ---- commands from the app ---- */
		if (ble_cmd_get(cmd)) {
			if (strcmp(cmd, "RESETMM") == 0) {
				have_minmax = 0;               /* next loaded reading starts fresh */
			}
		}

		/* ---- min / max tracking (done here, on the ATmega) ---- */
		if (p_mW >= NO_LOAD_mW) {
			if (!have_minmax) {
				p_min_mW = p_max_mW = p_mW;
				have_minmax = 1;
			} else {
				if (p_mW < p_min_mW) p_min_mW = p_mW;
				if (p_mW > p_max_mW) p_max_mW = p_mW;
			}
		}

		/* ---- send ---- */
		if (t % 10 == 0) {
			ble_send_axis(0, 8000);   /* graph 0-8 W: rated max 7.5 VA */
		}
		ble_send_reading(t, vrms_mV, irms_mA, p_mW, pf_milli);
		if (have_minmax) {
			ble_send_minmax(p_min_mW, p_max_mW);
		}

		t++;
		_delay_ms(1000);
	}
}
