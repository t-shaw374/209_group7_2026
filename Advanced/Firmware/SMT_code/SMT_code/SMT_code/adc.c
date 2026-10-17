/*
 * adc.c
 *
 * Created: 14/10/2026 12:43:33 pm
 *  Author: tsha374
 */ 
#include "adc.h"
#include <avr/io.h>

void adc_init(void)
{
	ADMUX = 0;                       // Aref reference
	DIDR0 |= (1 << ADC1D);                      // disable digital buffer on ADC1 (PC1)
	ADCSRA = (1 << ADEN) | (1 << ADPS2);        // enable, prescaler 16 -> 125 kHz at 2 MHz
}

uint16_t adc_read(uint8_t channel)
{
	// Keep reference bits, replace channel bits (MUX3:0)
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);

	ADCSRA |= (1 << ADSC);                      // start conversion
	while (ADCSRA & (1 << ADSC));               // wait until finished

	return ADC;                                 // 10-bit result (0..1023)
}

uint16_t adc_to_mv(uint16_t raw)
{
	// Rounded: mV = raw * 3300 / 1024
	uint16_t voltage_mV = (uint16_t)(((uint32_t)raw * ADC_VREF_MV + (ADC_STEPS / 2)) / ADC_STEPS);
	return voltage_mV;
}

void adc_mv_to_str(uint16_t mv, char *buf)
{
	if (mv > 9999) mv = 9999;                   // clamp to 4 digits

	// Fill digits from least significant to most significant
	buf[3] = '0' + (mv % 10); mv /= 10;         // ones
	buf[2] = '0' + (mv % 10); mv /= 10;         // tens
	buf[1] = '0' + (mv % 10); mv /= 10;         // hundreds
	buf[0] = '0' + mv;                          // thousands
	buf[4] = '\0';                              // null terminator
}