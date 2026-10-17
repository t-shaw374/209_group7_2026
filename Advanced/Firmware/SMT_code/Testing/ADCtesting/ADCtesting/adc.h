#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#define ADC_OFFSETS  10   // phase offsets, 12.5 us apart
#define ADC_SAMPLES  16   // samples per period, never more than this
#define ADC_POINTS   (ADC_SAMPLES * ADC_OFFSETS)   // always 160

// Marker for a slot that was not captured (see adc_capture)
#define ADC_EMPTY    0xFFFFu

// Row numbers in adc_buf
#define ADC_VOLT  0
#define ADC_CURR  1

// ADC mux channels (ADC0/PC0 is the zero-crossing input, so it is not used)
#define ADC_CH_V  1   // voltage signal on PC1
#define ADC_CH_I  2   // current signal on PC2

// Reconstructed period for both signals, always ADC_POINTS long.
// adc_buf[s][i] is the sample taken i * 12.5 us after the zero crossing (plus a
// small constant), so voltage and current values with the same i are the same
// instant. A slot that could not be captured holds ADC_EMPTY.
extern volatile uint16_t adc_buf[2][ADC_POINTS];

// Configure ADC, Timer0 and the PC0 zero-crossing interrupt. Call before sei().
void adc_init(void);

// Select the ADC channel for the next capture (1..7, 0 is reserved).
// Only call between captures.
void adc_set_channel(uint8_t ch);

// Blocking, needs interrupts on. Captures voltage and current for every offset
// (voltage in one period, current in the next) and fills adc_buf. Sampling stops
// after the 16th sample or at the next zero crossing, whichever comes first, so
// if the period is a bit short (15 samples fit) the missing slots stay ADC_EMPTY.
void adc_capture(void);

#endif