/*
 * adc.h
 *
 * Created: 14/10/2026 12:43:42 pm
 *  Author: tsha374
 */ 


#ifndef ADC_H_
#define ADC_H_

#include <stdint.h>

#define ADC_VREF_MV   3300UL   // must equal the voltage on the AREF pin
#define ADC_STEPS     1024UL   // 2^10
#define ADC_STR_LEN   5        // 4 digits + '\0'

// Timer1 + ADC setup (external AREF, auto-trigger on Timer1 compare B).
// Sampling doesn't start until adc_start_capture() and a zero crossing.
void adc_timer_init(void);

// Zero crossing input on PC0 (pin-change interrupt). Call before sei().
void zc_init(void);

// Begin a full capture (10 voltage/current pairs, 160 points per channel).
// Pauses the 7-seg scan until it finishes.
void adc_start_capture(void);

// Abort a capture in progress and resume the 7-seg scan.
void adc_abort_capture(void);

// 1 once the capture is complete
uint8_t adc_capture_done(void);

// Read one collected sample: channel 0 = voltage, 1 = current, index 0..159.
// Only valid once adc_capture_done() returns 1.
uint16_t adc_get_raw(uint8_t channel, uint8_t index);

// Convert raw ADC value to millivolts
uint16_t adc_to_mv(uint16_t raw);

// Convert millivolts to exactly 4 digits, zero-padded, e.g. "0825".
// buf must hold at least ADC_STR_LEN bytes.
void adc_mv_to_str(uint16_t mv, char *buf);



#endif /* ADC_H_ */