#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#define ADC_OFFSETS  10   // phase offsets, 12.5 us apart
#define ADC_SAMPLES  16   // samples per capture (one period), always exactly this many
#define ADC_POINTS   (ADC_SAMPLES * ADC_OFFSETS)   // 160

// Row numbers in adc_buf
#define ADC_VOLT  0
#define ADC_CURR  1

// ADC mux channels (ADC0/PC0 is the zero-crossing input, so it is not used)
#define ADC_CH_V  1   // voltage signal on PC1
#define ADC_CH_I  2   // current signal on PC2

// Reconstructed period for both signals, always ADC_POINTS (160) entries each.
// adc_buf[s][i] is the sample taken i * 12.5 us after the zero crossing (plus a
// small constant), so the voltage and current values with the same i belong to
// the same instant. If the real period is a little shorter than 2.000 ms, the
// last few points of each offset land just past the end of the period.
extern volatile uint16_t adc_buf[2][ADC_POINTS];

// Configure ADC, Timer0 and the PC0 zero-crossing interrupt. Call before sei().
void adc_init(void);

// Select the ADC channel (1..7, 0 is reserved for the zero-crossing pin).
// Only call while no capture is running.
void adc_set_channel(uint8_t ch);

// Blocking, needs interrupts on. Fills both rows of adc_buf.
// 20 captures (10 offsets x voltage/current) of one signal period each. Each
// starts on the next zero crossing, but the last offsets have to wait one extra
// period, so the whole thing takes about 24 periods (roughly 50 ms).
void adc_capture(void);

#endif