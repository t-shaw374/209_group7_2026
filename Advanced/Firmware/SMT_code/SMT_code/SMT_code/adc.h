#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#define ADC_OFFSETS    20   // phase offsets per sample interval
#define ADC_SAMPLES    16   // samples per capture (one period), always exactly this many
#define ADC_POINTS     (ADC_SAMPLES * ADC_OFFSETS)   // 160
#define ADC_PER_COUNT  8    // periods averaged by adc_measure_period()

// Row numbers in adc_buf
#define ADC_VOLT  0
#define ADC_CURR  1

// ADC mux channels (ADC0/PC0 is the zero-crossing input, so it is not used)
#define ADC_CH_V  1   // voltage signal on PC1
#define ADC_CH_I  2   // current signal on PC2

// Reconstructed period for both signals, always ADC_POINTS (160) entries each.
// The points are evenly spaced over one measured period (period / 160 apart,
// starting a small constant after the zero crossing), so the voltage and current
// values with the same index belong to the same instant. Because the spacing is
// rounded to whole timer ticks, the 160 points span the period to within ~4 us.
extern volatile uint16_t adc_buf[2][ADC_POINTS];

// Configure ADC, Timer1, Timer4 and the PC0 zero-crossing interrupt. Call before sei().
void adc_init(void);

// Select the ADC channel (1..7, 0 is reserved for the zero-crossing pin).
// Only call while no capture is running.
void adc_set_channel(uint8_t ch);

// Blocking, needs interrupts on. Do not call while adc_capture() is running.
// Measures ADC_PER_COUNT periods of the zero-crossing signal with Timer4.
// Returns the total in Timer4 ticks (0.5 us each), or 0 if there was no signal
// or it was too slow (period above ~3.6 ms). Takes up to 9 periods.
// adc_capture() calls this itself; you only need it for a standalone reading.
uint16_t adc_measure_period(void);

// Blocking, needs interrupts on. Measures the period, sets all sample timing
// from it, then fills both rows of adc_buf with 20 captures (10 offsets x
// voltage/current). Takes about 33 periods in total.
// Returns 1 on success, 0 if there was no signal or the frequency is outside
// about 275..568 Hz (adc_buf is then left unchanged).
// Note: if the signal disappears in the middle of a capture, this still waits.
uint8_t adc_capture(void);

// Period used for the last successful capture: ADC_PER_COUNT periods in 0.5 us
// ticks.  frequency in Hz = 16000000 / ticks,  period in us = ticks / 16
uint16_t adc_period_ticks(void);

#endif