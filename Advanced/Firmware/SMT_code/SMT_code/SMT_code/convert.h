/*
 * convert.h
 *
 * Created: 18/10/2026 10:01:04 pm
 *  Author: thoma
 */ 


#ifndef CONVERT_H_
#define CONVERT_H_


#include <stdint.h>
#include "adc.h"

// Converts the raw ADC counts in adc_buf to real values, in place:
//   row ADC_VOLT -> voltage in mV  (int16_t, signed), see V_INCLUDE_SHUNT
//   row ADC_CURR -> load current in mA  (int16_t, signed)
//
// Zero for both channels is the offset reference on pin 22 (ADC7), measured
// at the start of every conversion. Both difference amps pass that offset
// through 1:1, so the reference reading is each channel's zero.

// ---- Hardware values (change here if the circuit changes) ----------------
#define AREF_MV       3307   // ADC reference, mV (3300 default) (measure it for best accuracy)

#define VDIV_RTOP     2717    // voltage divider: Vvs = V * RBOT / (RTOP + RBOT)
#define VDIV_RBOT     150
#define VAMP_RF       100    // voltage diff amp gain = RF / RIN (R19 / R20)
#define VAMP_RIN      82

#define SHUNT_MOHM    500    // current shunt, milliohms
#define IAMP_RF       3391     // (default 3300) current diff amp gain = RF / RIN (R11 / R9)
#define IAMP_RIN      1000

#define OFFSET_CH     7      // ADC channel of the offset reference (pin 22)

// Which voltage the firmware reports.
// The divider sits across the load only, with the shunt in series below it.
//   0: load voltage (what the divider sees)
//   1: source voltage = load voltage + shunt drop, added point by point
//      (v + i * Rshunt at every instant, so phase is exact). Matches a single
//      scope probe across the AC source. Power then includes the shunt's I^2 R.
#define V_INCLUDE_SHUNT  1

// Optional per-channel zero corrections, in 1/16 ADC counts. Find them once
// with the source off: (channel average - offset reading) * 16. Leave at 0
// unless max current needs to be better than ~1-2 %.
#define V_ZERO_CORR16 0
#define I_ZERO_CORR16 0

// ---- Converted data -------------------------------------------------------
// After convert_to_real(), read the buffers through these (signed values).
#define REAL_MV  ((volatile int16_t *)adc_buf[ADC_VOLT])
#define REAL_MA  ((volatile int16_t *)adc_buf[ADC_CURR])

// Measure the offset reference: 16 conversions on OFFSET_CH, returned as their
// sum (= average x 16, for sub-count precision). Blocking, about 2 ms.
// Only call while no capture is running.
uint16_t convert_measure_offset(void);

// Measure the offset, then replace every value in adc_buf with mV / mA.
// Call once after a successful adc_capture() and before the data is used.
// Calling it twice would convert already-converted values.
void convert_to_real(void);

// Offset reference from the last convert_to_real(), in mV.
// Should be close to 2100; far away from that means a hardware fault.
uint16_t convert_offset_mv(void);

#endif /* CONVERT_H_ */