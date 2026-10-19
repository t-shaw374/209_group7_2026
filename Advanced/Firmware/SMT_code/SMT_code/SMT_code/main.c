#include "common.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "i2c.h"
#include "sh1106.h"
#include "graph.h"
#include "adc.h"
#include "convert.h"
#include "calc.h"
#include "format.h"
#include "timeout.h"
#include "segdisplay.h"
#include "uart.h"

typedef enum {
	STATE_INIT = 0,
	STATE_PERIOD,
	STATE_ADC,
	STATE_CALC,
	STATE_UART,
	STATE_7SEG,
	STATE_OLED,
	STATE_ERROR
} state_t;

static state_t g_state = STATE_INIT;

/* Call from inside any state to move to another one */
#define SET_STATE(s)    (g_state = (s))

static calc_result_t g_result;                   // Vrms, power, Imax of the last capture
static graph_mode_t  g_show = GRAPH_POWER;       // what the 7-seg and OLED show this loop

static uint8_t oled_ok = 0;


// Send the three results as one line, e.g. "Vrms 10618 mV, Imax 405 mA, P 2625 mW"
static void uart_send_results(const calc_result_t *r)
{
	char line[56];
	char *p = line;
	const char *s;

	for (s = "Vrms ";   *s; s++) *p++ = *s;
	p = fmt_int(p, r->vrms_mv);
	for (s = " mV, Imax "; *s; s++) *p++ = *s;
	p = fmt_int(p, r->imax_ma);
	for (s = " mA, P "; *s; s++) *p++ = *s;
	p = fmt_int(p, r->power_mw);
	for (s = " mW\r\n"; *s; s++) *p++ = *s;
	*p = '\0';

	uart_puts(line);
}

// All three results for the OLED's bottom row, e.g. "10.62V 0.405A 2.625W"
static void results_line(const calc_result_t *r, char *p)
{
	p = fmt_milli(p, r->vrms_mv, 'V');
	*p++ = ' ';
	p = fmt_milli(p, r->imax_ma, 'A');
	*p++ = ' ';
	p = fmt_milli(p, r->power_mw, 'W');
	*p = '\0';
}

// Send all 160 points, one line per instant: "voltage current"
// raw = 1: ADC counts (call BEFORE convert_to_real)
// raw = 0: mV and mA  (call AFTER convert_to_real)
//static void uart_dump_buffer(uint8_t raw)
//{
	//char line[16];
//
	//uart_puts(raw ? "--- raw counts ---\r\n" : "--- mV mA ---\r\n");
	//for (uint8_t k = 0; k < ADC_POINTS; k++) {
		//char *p = line;
		//if (raw) {
			//p = fmt_int(p, adc_buf[ADC_VOLT][k]);
			//*p++ = ' ';
			//p = fmt_int(p, adc_buf[ADC_CURR][k]);
			//} else {
			//p = fmt_int(p, REAL_MV[k]);
			//*p++ = ' ';
			//p = fmt_int(p, REAL_MA[k]);
		//}
		//*p++ = '\r';
		//*p++ = '\n';
		//*p = '\0';
		//uart_puts(line);
	//}
	//uart_puts("\r\n");
//}


static void state_machine_run(void)
{
	uint8_t present;
	uint8_t ok;

	switch (g_state) {

		case STATE_INIT:
		init_display(128);
		i2c_init();

		oled_ok = sh1106_probe();
		if (oled_ok) sh1106_init();

		uart_init();
		adc_init();
		timeout_init();

		sei();
		SET_STATE(STATE_ADC);
		break;

		case STATE_PERIOD:
		_delay_ms(1000);                         // time between measurements
		g_show = (graph_mode_t)((g_show + 1) % GRAPH_MODES);   // power -> voltage -> current
		SET_STATE(STATE_ADC);
		break;

		case STATE_ADC:
		display_pause();
		ok = adc_capture();
		display_resume();

		SET_STATE(ok ? STATE_CALC : STATE_ERROR);
		break;

		case STATE_CALC:
		convert_to_real();
		calc_run(&g_result);
		SET_STATE(STATE_UART);
		break;

		case STATE_UART:
		uart_send_results(&g_result);
		SET_STATE(STATE_7SEG);
		break;

		case STATE_7SEG:
		{
			uint32_t m;
			switch (g_show) {
				case GRAPH_VOLTAGE: m = g_result.vrms_mv; break;
				case GRAPH_CURRENT: m = g_result.imax_ma; break;
				default:            // power: no minus sign on the 7-seg, show size
				m = (g_result.power_mw < 0) ? -g_result.power_mw : g_result.power_mw;
				break;
			}
			uint8_t  dp;
			uint16_t n = fmt_4digit(m, &dp);
			seperate_and_load_characters(n, dp);
		}
		SET_STATE(STATE_OLED);
		break;

		case STATE_OLED:
		present = sh1106_probe();
		if (present && !oled_ok) {
			sh1106_init();            // just appeared (or came back after a power glitch)
		}
		oled_ok = present;

		if (oled_ok) {
			char vals[24];
			results_line(&g_result, vals);
			graph_render(g_show, vals);
		}

		SET_STATE(STATE_PERIOD);
		break;

		case STATE_ERROR:
		uart_puts("No signal\r\n");
		SET_STATE(STATE_PERIOD);                 // wait, then try another capture
		break;

		default:
		SET_STATE(STATE_ERROR);
		break;
	}
}

int main(void)
{
	while (1) {
		state_machine_run();
	}
}