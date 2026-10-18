/*
 * doom.c  -  ATmega side of DOOM mode (see doom.h, game itself in doom_core.c)
 */

#include "common.h"
#if ENABLE_DOOM


#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>
#include "common.h"
#include "pgm_compat.h"
#include "doom.h"
#include "doom_core.h"
#include "oled.h"
#include "display.h"
#include "ble_cmd.h"
#include "ble_report.h"
#include "util.h"

static const uint8_t seg_doom[4] = { 0x5E, 0x3F, 0x3F, 0x54 };   /* "dOOn" (no M on 7 segments) */

/* ---- DOOM turbo --------------------------------------------------------------
 * The ATmega's clock is 16 MHz divided by 8 (CKDIV8) = 2 MHz in all labs. For the
 * game the divider is switched to 1 (16 MHz, fine at 5 V), and switched back on
 * EXIT. Everything clocked from it is adjusted to keep the same real-world rates:
 *   UART  UBRR 12 -> 103   (9600 baud either way)
 *   Timer0 /64, 78 counts -> /256, 156 counts  (2.5 ms tick either way)
 *   I2C   clock stretched to stay near 400 kHz
 * Measurement (ADC, Timer1) is idle in DOOM mode.
 * Set DOOM_TURBO to 0 to stay at 2 MHz (about 8 frames/s instead of ~35).
 */
#ifndef DOOM_TURBO
#define DOOM_TURBO 1
#endif

#if DOOM_TURBO
static void set_clock(uint8_t turbo)
{
	/* let the last UART byte finish (~1 ms at 9600 baud) at the current speed */
	while (!(UCSR0A & (1 << UDRE0))) { ; }
	_delay_loop_2(turbo ? 600 : 4800);            /* 4 cycles each: 1.2 ms */

	uint8_t sreg = SREG;
	cli();
	CLKPR = (1 << CLKPCE);                        /* timed sequence: next write within 4 cycles */
	CLKPR = turbo ? 0 : 3;                        /* divide by 1 or by 8 */
	UBRR0H = 0;
	UBRR0L = turbo ? 103 : UBRR;
	TCNT0  = 0;
	TCCR0B = turbo ? (1 << CS02) : ((1 << CS01) | (1 << CS00));
	OCR0A  = turbo ? 155 : 77;
	SREG = sreg;
	oled_set_fast(turbo);
}
#endif

static void report(const char *what)
{
	ble_send_P(PSTR("DOOM,"));
	ble_send_P(what);                         /* flash string */
	ble_send_P(PSTR(",kills="));
	ble_send_uint(doom_kills());
	ble_send_P(PSTR(",hp="));
	ble_send_uint((uint32_t)(doom_health() < 0 ? 0 : doom_health()));
	ble_send_P(PSTR("\n"));
}

void doom_run(void)
{
	if (!oled_present()) {
		ble_send_P(PSTR("ERR,DOOM,no OLED found on PB0/PB1\n"));
		return;
	}
	ble_send_P(PSTR("OK,DOOM\n"));
	display_raw(seg_doom);
#if DOOM_TURBO
	set_clock(1);
#endif

	doom_reset((uint16_t)ticks());
	uint32_t last = ticks();
	uint16_t shown_kills = 0xFFFF;
	char cmd[BLE_CMD_MAX];

	while (1) {
		while (ble_cmd_get(cmd)) {
			str_upper(cmd);
			if (!strcmp(cmd, "EXIT")) {
				oled_clear();
#if DOOM_TURBO
				set_clock(0);
#endif
				ble_send_P(PSTR("OK,EXIT\n"));
				return;
			}
			if (cmd[0] && !cmd[1]) doom_key(cmd[0]);       /* single-letter key */
		}

		uint32_t now = ticks();
		uint16_t dt = (uint16_t)((now - last) * 5u / 2u);   /* 2.5 ms ticks -> ms */
		last = now;
		if (dt > 200) dt = 200;

		doom_update(dt);
		doom_render();

		uint8_t ev = doom_take_events();
		if (ev & DOOM_EV_KILL) report(PSTR("kill"));
		if (ev & DOOM_EV_DEAD) report(PSTR("dead"));

		if (doom_kills() != shown_kills) {
			shown_kills = doom_kills();
			seperate_and_load_characters(shown_kills, 4);   /* 7-seg = kill counter */
		}
	}
}

#endif /* ENABLE_DOOM */
