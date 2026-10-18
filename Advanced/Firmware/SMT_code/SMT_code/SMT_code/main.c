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
	STATE_CALC,
	STATE_UART,
	STATE_7SEG,
	STATE_OLED,
	STATE_ERROR
} state_t;

static state_t g_state = STATE_INIT;

/* Call from inside any state to move to another one */
#define SET_STATE(s)    (g_state = (s))


// 9600 baud at 2 MHz, normal speed: UBRR = F_CPU/(16*baud) - 1 = 12 (0.2% error)
#define UART_BAUD           9600UL
#define UART_UBRR           ((uint16_t)((F_CPU / (16UL * UART_BAUD)) - 1))

static uint16_t temp = 1234;
static char buf[5];

static uint8_t oled_ok = 0;


void mv_to_str(uint16_t mv, char *buf)
{
	if (mv > 9999) mv = 9999;                   // clamp to 4 digits

	// Fill digits from least significant to most significant
	buf[3] = '0' + (mv % 10); mv /= 10;         // ones
	buf[2] = '0' + (mv % 10); mv /= 10;         // tens
	buf[1] = '0' + (mv % 10); mv /= 10;         // hundreds
	buf[0] = '0' + mv;                          // thousands
	buf[4] = '\0';                              // null terminator
}


static void state_machine_run(void)
{
	uint8_t present;                 
	
	switch (g_state) {

		case STATE_INIT:
		init_display(128);
		i2c_init();
		
		
		oled_ok = sh1106_probe();
		if (oled_ok) sh1106_init();
		
		uart_init();
		adc_init();

		sei();
		SET_STATE(STATE_ADC);
		break;

		case STATE_PERIOD:
		temp++;
		_delay_ms(1000);                 // sample period
		SET_STATE(STATE_ADC);
		break;

		case STATE_ADC:
		display_pause();
		adc_capture();  
		display_resume();                     
		
		SET_STATE(STATE_CALC);
		break;

		case STATE_CALC:
		SET_STATE(STATE_UART);
		break;

		case STATE_UART:
		//one line per instant: voltage,current  (CR+LF), blank line after the set
		for (uint8_t i = 0; i < ADC_POINTS; i++) {
			uart_put_u16(adc_buf[ADC_VOLT][i]);
			uart_putc(',');
			uart_put_u16(adc_buf[ADC_CURR][i]);
			uart_puts("\r\n");
		}
		uart_puts("\r\n");


		SET_STATE(STATE_7SEG);
		break;

		case STATE_7SEG:
		seperate_and_load_characters(temp, 0);
		SET_STATE(STATE_OLED);
		break;

		case STATE_OLED:
		present = sh1106_probe();
		if (present && !oled_ok) {
			sh1106_init();            // just appeared (or came back after a power glitch)
		}
		oled_ok = present;

		if (oled_ok) {
			mv_to_str(temp, buf);
			graph_render(buf);
		}
		
		SET_STATE(STATE_PERIOD);
		break;

		case STATE_ERROR:
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