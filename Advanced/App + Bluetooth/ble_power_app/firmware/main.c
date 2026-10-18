/*
 * main.c  -  COMPSYS209 Team 07 energy monitor
 *
 * Built from the lab code in taye752/209-Labs and t-shaw374/compsys209_labs:
 *   Lab 4  ADC (125 kHz ADC clock)            -> sampler.c (Timer1-triggered, 3.3 V AREF)
 *   Lab 4  UART at 9600 baud                  -> UART.c (unchanged)
 *   Lab 5  zero-crossing timing               -> sampler.c (frequency, now on PC0)
 *   Lab 6  7-segment display + Timer0         -> display.c (+ unit letters, tick)
 * plus a 128 x 32 OLED power graph (oled.c, oled_graph.c) and DOOM mode (doom*.c),
 * plus BLE reporting (ble_report.c), app commands (ble_cmd.c, commands.c) and
 * EEPROM calibration (settings.c).
 *
 * Pins (PCB_TEAM07_SMT netlist):
 *   ADC   PC1 Vvm (voltage), PC2 Vim (current), PC3 Vipk, PE3 offset, AREF = 3.3 V
 *   PC0   VvmCZ zero-crossing comparator (pin-change interrupt)
 *   UART  PD0 RX / PD1 TX -> TXS0108E level shifter -> ESP32 (header P5)
 *   7-seg PB0 SH_CP, PB1 SH_ST, PB2 SH_DS, PD7..PD4 digits DS1..DS4, PD3 OE (header P6)
 *   OLED  PC4 SDA / PC5 SCL, hardware TWI (header P4)
 *   ISP   PB3 MOSI, PB4 MISO, PB5 SCK, PC6 RESET (header P1, also reachable by the ESP32)
 *
 * Every report period (default 1 s):
 *   1. capture WINDOWS x N_PAIRS sample pairs (160 ms, a whole number of cycles)
 *   2. compute Vrms, Irms, P, S, PF            (measure.c)
 *   3. integrate energy, update min / max, measure frequency
 *   4. update the display and send A / D / M lines to the app
 */

#include "common.h"
#include "pgm_compat.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include "UART.h"
#include "ble_report.h"
#include "ble_cmd.h"
#include "display.h"
#include "sampler.h"
#include "measure.h"
#include "settings.h"
#include "monitor.h"
#include "oled.h"
#include "oled_graph.h"
#include "doom.h"
#include "util.h"
#include "scratch.h"

#define TICKS_PER_S     400u      /* 2.5 ms ticks */
#define DISP_ROTATE_T   800u      /* AUTO display: next quantity every 2 s */
#define AXIS_EVERY      10u       /* resend the graph axis every 10 reports */

monitor_t mon;

/* ADC capture buffers live in the shared scratch buffer (DOOM reuses it) */
#define vbuf (scratch_buf)
#define ibuf (scratch_buf + N_PAIRS + 1)
#if (2 * N_PAIRS + 1) > SCRATCH_WORDS
#error "scratch buffer too small for N_PAIRS"
#endif

static uint32_t to_milli(float x)
{
	return (x <= 0) ? 0 : (uint32_t)(x * 1000.0f + 0.5f);
}

static void show(uint8_t item)
{
	if (!mon.have_last) {
		display_value(0, SEG_DASH);
		return;
	}
	switch (item) {
		case DISP_V:  display_value(to_milli(mon.last.vrms), SEG_U); break;
		case DISP_I:  display_value(to_milli(mon.last.irms), SEG_A); break;
		case DISP_P:  display_value(to_milli(mon.last.p),    SEG_P); break;
		case DISP_E:  display_value(to_milli(mon.energy_Wh), SEG_E); break;
		case DISP_PF: display_value(to_milli(mon.last.pf),   SEG_F); break;
	}
}

/* OLED: text line "3.41W 13.0V 0.264A" above the scrolling power graph */
static void oled_update(void)
{
	char t[24];
	uint8_t n = 0;
	n += fmt_fixed(t + n, to_milli(mon.last.p), 2);    t[n++] = 'W'; t[n++] = ' ';
	n += fmt_fixed(t + n, to_milli(mon.last.vrms), 1); t[n++] = 'V'; t[n++] = ' ';
	n += fmt_fixed(t + n, to_milli(mon.last.irms), 3); t[n++] = 'A';
	graph_draw(t, n);
}

static void measure_now(meas_result_t *r)
{
	meas_sums_t sums;
	meas_clear(&sums);
	for (uint8_t w = 0; w < WINDOWS; w++) {
		sampler_capture(vbuf, ibuf);
		meas_accumulate(&sums, vbuf, ibuf, N_PAIRS);
	}
	meas_compute(&sums, &settings.cal, r);
}

int main(void)
{
	usart_init(UBRR);
	ble_cmd_init();
	init_display();
	display_timer_init();
	sampler_init();
	settings_load();
	sei();
	oled_init();                 /* carries on without an OLED if none answers */
	graph_clear();

	memset(&mon, 0, sizeof mon);
	mon.axis_dirty = 1;
	mon.t0_ticks = ticks();
	display_value(0, SEG_DASH);

	ble_send_P(PSTR("BOOT," FW_VERSION "\n"));

	uint32_t now          = ticks();
	uint32_t next_report  = now;
	uint32_t last_report  = now;
	uint32_t disp_tick    = now;
	uint8_t  disp_item    = DISP_V;
	uint8_t  report_count = 0;
	char     cmd[BLE_CMD_MAX];

	(void)sampler_take_zc_count();

	while (1) {
		/* ---- commands from the app ---- */
		while (!mon.doom_request && ble_cmd_get(cmd)) {   /* keys after DOOM go to the game */
			commands_handle(cmd);
		}

		/* ---- DOOM mode (blocks until EXIT) ---- */
		if (mon.doom_request) {
			mon.doom_request = 0;
#if ENABLE_DOOM
			doom_run();
#endif
			now = ticks();
			next_report = last_report = disp_tick = now;   /* don't count game time as energy */
			(void)sampler_take_zc_count();
			show(mon.disp_mode == DISP_AUTO ? disp_item : mon.disp_mode);
			if (mon.have_last) oled_update();
		}

		now = ticks();

		/* ---- display ---- */
		if (now - disp_tick >= DISP_ROTATE_T) {
			disp_tick = now;
			if (++disp_item > DISP_E) disp_item = DISP_V;   /* AUTO: V, I, P, E */
			show(mon.disp_mode == DISP_AUTO ? disp_item : mon.disp_mode);
		}

		/* ---- measurement + report ---- */
		if ((int32_t)(now - next_report) < 0) continue;

		uint32_t rate_t = (uint32_t)settings.rate_ms * TICKS_PER_S / 1000u;
		next_report += rate_t;
		if ((int32_t)(now - next_report) >= 0) next_report = now + rate_t;  /* fell behind */

		measure_now(&mon.last);
		mon.have_last = 1;

		now = ticks();
		uint32_t dt_ticks = now - last_report;
		last_report = now;
		float dt_s = dt_ticks / (float)TICKS_PER_S;

		/* energy: E += P * dt (first report has no interval yet) */
		if (report_count > 0) {
			mon.energy_Wh += mon.last.p * dt_s / 3600.0f;
		}

		/* mains frequency from zero-crossing edges counted since the last report */
		uint16_t zc = sampler_take_zc_count();
		if (dt_s > 0.1f) {
			mon.freq_dHz = (uint16_t)(zc * 10.0f / dt_s + 0.5f);
		}

		/* min / max of real power, ignoring "no load" readings */
		uint32_t p_mW = to_milli(mon.last.p);
		if (p_mW >= NO_LOAD_mW) {
			if (!mon.have_mm) {
				mon.pmin_mW = mon.pmax_mW = p_mW;
				mon.have_mm = 1;
			} else {
				if (p_mW < mon.pmin_mW) mon.pmin_mW = p_mW;
				if (p_mW > mon.pmax_mW) mon.pmax_mW = p_mW;
			}
		}

		show(mon.disp_mode == DISP_AUTO ? disp_item : mon.disp_mode);

		/* ---- OLED graph ---- */
		graph_push(p_mW, settings.axis_max_mW);
		oled_update();

		/* ---- send to the app ---- */
		if (mon.axis_dirty || report_count % AXIS_EVERY == 0) {
			ble_send_axis(0, settings.axis_max_mW);
			mon.axis_dirty = 0;
		}
		ble_send_reading((now - mon.t0_ticks) * 5u / 2u,          /* ticks -> ms */
		                 to_milli(mon.last.vrms),
		                 to_milli(mon.last.irms),
		                 p_mW,
		                 (uint16_t)to_milli(mon.last.pf),
		                 to_milli(mon.energy_Wh));
		if (mon.have_mm) {
			ble_send_minmax(mon.pmin_mW, mon.pmax_mW);
		}
		report_count++;
	}
}
