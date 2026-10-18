/*
 * common.h  -  Team 07 energy monitor: clock, UART and hardware calibration constants
 *
 * Based on Lab 4 common.h (TAYE752). F_CPU and UBRR are unchanged from the labs.
 */

#ifndef COMMON_H_
#define COMMON_H_

#define F_CPU 2000000UL          /* 16 MHz / CKDIV8, as in all labs */
#define UBRR  12                 /* 9600 baud at 2 MHz (0.16 % error) */

#define FW_VERSION "team07-1.0"

/* ---------------- analogue front end (measured parts, see tally) ----------------
 *
 * Voltage channel:  Vmeas_ac / Vin  = divider * diff-amp * RC filter
 *                   = 5.44/(119.7+5.44) * 0.9953 * 0.9987           = 0.04321
 * Current channel:  Imeas_ac / Iload = Rshunt * I-amp gain * RC filter
 *                   = 0.505 ohm * 2.5746 * 0.9987                   = 1.2986 V/A
 *
 * These are the design values from the measured resistors. Fine-tune with the
 * CALV / CALI commands against a multimeter; the correction is saved in EEPROM.
 */
#define V_RATIO        0.04321f  /* Vmeas (AC part) per volt of Vin                */
#define I_RATIO        1.2986f   /* Vmeas (AC part) per amp of load current [V/A]  */
#define ADC_VREF       4.98f     /* AVCC = board 5 V rail, measured 4.98 V          */

/* ---------------- sampling ----------------
 * Timer1 triggers one ADC conversion every SAMPLE_US, alternating V, I, V, I ...
 * 200 us gives each channel 2500 samples/s = exactly 5 samples per 500 Hz cycle,
 * so every block of N_PAIRS (a multiple of 5) covers a whole number of cycles.
 * The ADC itself runs at 125 kHz (prescaler 16, as in Lab 4): 108 us/conversion.
 */
#define MAINS_HZ       500.0f
#define SAMPLE_US      200
#define N_PAIRS        100       /* 100 pairs = 40 ms = 20 cycles per window       */
#define WINDOWS        4         /* windows averaged per report (160 ms of data)   */

/* Readings below these are treated as "no load" (noise). */
#define NO_LOAD_mA     5
#define NO_LOAD_mW     50

/* ---------------- defaults (changeable with commands, kept in EEPROM) ---------- */
#define DEFAULT_RATE_MS     1000
#define DEFAULT_AXIS_MAX_mW 8000  /* 0-8 W graph: rated max 7.5 VA */

/* ---------------- extras ----------------
 * DOOM mode on the OLED. Set to 0 for the assessed build: the game code is then
 * left out completely and the DOOM command replies "ERR,DOOM,disabled".
 */
#ifndef ENABLE_DOOM
#define ENABLE_DOOM 1
#endif

#endif /* COMMON_H_ */
