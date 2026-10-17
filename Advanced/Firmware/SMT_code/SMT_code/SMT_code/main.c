#include "common.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "i2c.h"
#include "sh1106.h"
#include "graph.h"
#include "adc.h"
#include "segdisplay.h"
#include "uart.h"

typedef enum {
	STATE_INIT = 0,
	STATE_PERIOD,
	STATE_ADC,
	STATE_ADC_WAIT,
	STATE_CALC,
	STATE_UART,
	STATE_7SEG,
	STATE_OLED,
	STATE_ERROR
} state_t;

static state_t g_state = STATE_INIT;

/* Call from inside any state to move to another one */
#define SET_STATE(s)    (g_state = (s))


// A full capture takes about 40-80 ms when the zero crossing is present,
// so this only ever expires if the crossing is missing.
#define CAPTURE_TIMEOUT_MS  500U
#define ERROR_RETRY_MS      1000U

// 9600 baud at 2 MHz, normal speed: UBRR = F_CPU/(16*baud) - 1 = 12 (0.2% error)
#define UART_BAUD           9600UL
#define UART_UBRR           ((uint16_t)((F_CPU / (16UL * UART_BAUD)) - 1))

static uint16_t mv = 0;
static char mv_str[ADC_STR_LEN];
static uint16_t capture_wait_ms = 0;

static void state_machine_run(void)
{
	switch (g_state) {

		case STATE_INIT:
		init_display(128);
		i2c_init();
		sh1106_init();
		adc_timer_init();
		zc_init();
		usart_init(UART_UBRR);
		sei();
		SET_STATE(STATE_ADC);
		break;

		case STATE_PERIOD:
		_delay_ms(1000);                 // sample period
		SET_STATE(STATE_ADC);
		break;

		case STATE_ADC:
		capture_wait_ms = 0;
		adc_start_capture();                   // pauses the 7-seg until done
		SET_STATE(STATE_ADC_WAIT);
		break;

		case STATE_ADC_WAIT:
		if (adc_capture_done()) {
			SET_STATE(STATE_CALC);
		}
		
		break;

		SET_STATE(STATE_CALC);
		break;

		case STATE_CALC:
		
		adc_mv_to_str(mv, mv_str);   // "0825" style string for the OLED
		SET_STATE(STATE_UART);
		break;

		case STATE_UART:
		// One row per timestep: "VVVV IIII\r\n", voltage then current in mV
		for (uint8_t i = 0; i < ADC_NUM_POINTS; i++) {
			send_mV_value(adc_to_mv(adc_get_raw(0, i)));   // voltage
			usart_transmit(' ');
			send_mV_value(adc_to_mv(adc_get_raw(1, i)));   // current
			uart_tx_string("\r\n");
		}
		SET_STATE(STATE_7SEG);
		break;

		case STATE_7SEG:
		seperate_and_load_characters(mv, 0);
		SET_STATE(STATE_OLED);
		break;

		case STATE_OLED:
		graph_render(mv_str);
		SET_STATE(STATE_PERIOD);
		break;

		case STATE_ERROR:
		seperate_and_load_characters(8888, 0);  // 8888 on the 7-seg = capture failed
		_delay_ms(ERROR_RETRY_MS);
		SET_STATE(STATE_ADC);                   // try another capture
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