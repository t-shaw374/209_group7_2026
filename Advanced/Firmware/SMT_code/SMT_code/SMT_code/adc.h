/*
 * adc.h
 *
 * Created: 14/10/2026 12:43:42 pm
 *  Author: tsha374
 */ 


#ifndef ADC_H_
#define ADC_H_

#include <stdint.h>

#define ADC_VREF_MV   3300UL
#define ADC_STEPS     1024UL   // 2^10
#define ADC_STR_LEN   5        // 4 digits + '\0'

// Initialise ADC: AVcc reference, prescaler 16
void adc_init(void);

// Read a single channel (0-7), returns raw 10-bit value (0..1023)
uint16_t adc_read(uint8_t channel);

// Convert raw ADC value to millivolts
uint16_t adc_to_mv(uint16_t raw);

// Convert millivolts to exactly 4 digits, zero-padded, e.g. "0825".
// buf must hold at least ADC_STR_LEN bytes.
void adc_mv_to_str(uint16_t mv, char *buf);



#endif /* ADC_H_ */