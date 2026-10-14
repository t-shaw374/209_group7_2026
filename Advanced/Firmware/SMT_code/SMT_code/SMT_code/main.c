/*
 * SMT_code.c
 *
 * Created: 14/10/2026 12:20:34 pm
 * Author : tsha374
 */ 

#include "common.h"   // defines F_CPU; must come before <util/delay.h>
#include <avr/io.h>
#include <util/delay.h>

#include "i2c.h"
#include "sh1106.h"
#include "graph.h"
#include "adc.h"

int main(void)
{
	i2c_init();
	sh1106_init();
	
	char mv_str[ADC_STR_LEN];                   // e.g. "1650"
	adc_init();

	while (1) {
		uint16_t raw = adc_read(1);             // ADC1
		uint16_t mv = adc_to_mv(raw);           // convert to millivolts

		adc_mv_to_str(mv, mv_str);              // 4-digit string
		
		graph_render(mv_str);
		_delay_ms(1000);
	}

}

