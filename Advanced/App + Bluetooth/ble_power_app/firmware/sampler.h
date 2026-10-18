/*
 * sampler.h  -  hardware-timed ADC capture of Vvm (PC1/ADC1) and Vim (PC2/ADC2)
 *               plus zero-crossing edge counting on VvmCZ (PC0, pin-change interrupt)
 *
 * Timer1 (CTC) triggers each conversion, so the sample spacing stays exact even
 * while the display and UART interrupts run. Based on Lab 4 adc.c (TAYE752):
 * same 125 kHz ADC clock (prescaler 16); the reference is the board's 3.3 V AREF.
 */

#ifndef SAMPLER_H_
#define SAMPLER_H_

#include <stdint.h>
#include "common.h"

void sampler_init(void);

/* Blocking: capture one window. v gets N_PAIRS+1 samples, i gets N_PAIRS. */
void sampler_capture(uint16_t *v, uint16_t *i);

/* Single blocking conversion on any channel (e.g. ADC_CH_OFFSET). Only call
 * when no capture is running. */
uint16_t sampler_read_channel(uint8_t channel);

/* Rising edges seen on Vzc since the last call (for frequency measurement). */
uint16_t sampler_take_zc_count(void);

#endif /* SAMPLER_H_ */
