#include "common.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "i2c.h"
#include "sh1106.h"
#include "graph.h"
#include "adc.h"
#include "segdisplay.h"
#include "zerocross.h"

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

static uint16_t g_raw = 0;
static uint16_t g_mv  = 0;
static char     g_mv_str[ADC_STR_LEN];

static void state_machine_run(void)
{
	switch (g_state) {

		case STATE_INIT:
		init_display(128);
		i2c_init();
		sh1106_init();
		adc_init();
		zc_init();
		sei();
		SET_STATE(STATE_ADC);
		break;

		case STATE_PERIOD:
		_delay_ms(1000);                 // sample period
		SET_STATE(STATE_ADC);
		break;

		case STATE_ADC:
		g_raw = adc_read(1);             // PC1 / ADC1
		g_mv = adc_to_mv(g_raw);
		SET_STATE(STATE_CALC);
		break;

		case STATE_CALC:
		
		adc_mv_to_str(g_mv, g_mv_str);   // "0825" style string for the OLED
		SET_STATE(STATE_7SEG);           // nothing to send over UART yet
		break;

		case STATE_UART:
		/* TODO: no UART code exists yet, so nothing routes here */
		SET_STATE(STATE_7SEG);
		break;

		case STATE_7SEG:
		seperate_and_load_characters(g_mv, 0);
		SET_STATE(STATE_OLED);
		break;

		case STATE_OLED:
		graph_render(g_mv_str);
		SET_STATE(STATE_PERIOD);
		break;

		case STATE_ERROR:
		/* TODO: decide what a fault should do (e.g. show 8888 and retry INIT) */
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