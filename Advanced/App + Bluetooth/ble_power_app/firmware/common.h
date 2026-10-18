/*
 * common.h  -  Team 07 energy monitor: clock, UART and hardware calibration constants
 *
 * Based on Lab 4 common.h (TAYE752). F_CPU and UBRR are unchanged from the labs.
 */

#ifndef COMMON_H_
#define COMMON_H_

#define F_CPU 2000000UL          /* 16 MHz / CKDIV8, as in all labs */
#define UBRR  12                 /* 9600 baud at 2 MHz (0.16 % error) */

#define FW_VERSION "team07-smt-1.0"

/* ---------------- board: PCB_TEAM07_SMT ----------------
 * Bare ATmega328PB with a 16 MHz crystal. Program the fuses once over the ISP
 * header (P1): external 8-16 MHz crystal + CKDIV8 -> 2 MHz, low fuse 0x7F.
 *
 * ADC pins:  PC1/ADC1 Vvm (voltage)   PC2/ADC2 Vim (current)
 *            PC3/ADC3 Vipk (current peak detector)   PE3/ADC7 offset (2.1Vdc net)
 *            PC0 VvmCZ = zero-crossing comparator, read as a digital input
 * ADC reference: the 3.3 V LDO is wired to AREF, so the ADC MUST use the external
 * AREF (REFS1:0 = 00). Selecting AVCC with a voltage on AREF connects the 5 V and
 * 3.3 V rails inside the chip.
 */
#define ADC_REFS_BITS  0         /* external AREF */
#define ADC_VREF       3.30f     /* measure TP7 (3.3Vdc) and put the real value here */
#define ADC_CH_V       1
#define ADC_CH_I       2
#define ADC_CH_IPK     3
#define ADC_CH_OFFSET  7

/* ---------------- analogue front end (LTspice_Simulation.asc values) ------------
 * Designators: simulation name [SMT schematic name]
 *
 * Voltage: divider Ra [R4] Vac->Vvs, Rb [R8] Vvs->Vis, then a difference amplifier
 *          In- : R13 [R20] from Vis, R15 [R19] feedback
 *          In+ : R14 [R22] from Vvs, R16 [R23] to the offset
 *   Vvm_ac = a * Rb/(Ra+Rb) * (Vac - Vis)  +  (a - b) * Vis
 *   a = R16/(R14+R16) * (1 + R15/R13),  b = R15/R13
 *   With R14 = 68k (as simulated) a != b and about 3.5 % of the shunt voltage leaks
 *   into the voltage reading at full load. R14 = 82k (matching R13) removes it.
 * Current: Rs [R6 || R7] = 0.5 ohm, gain R4/R2 [R11/R9] = 27k/12k = 2.25
 * Both outputs: 1k + 10n RC filter, gain 0.9995 at 500 Hz (use 10n on BOTH so the
 * phase shifts match).
 *
 * Fine-tune on the bench with CALV / CALI (saved in EEPROM).
 */
#define R_DIV_TOP  270e3f        /* Ra  [R4]  */
#define R_DIV_BOT  15e3f         /* Rb  [R8]  */
#define R_VN_IN    82e3f         /* R13 [R20] */
#define R_VN_FB    100e3f        /* R15 [R19] */
#define R_VP_IN    68e3f         /* R14 [R22]  -> 82e3f once R22 is changed to 82k */
#define R_VP_REF   100e3f        /* R16 [R23] */
#define R_SHUNT    0.5f          /* Rs  [R6 || R7] */
#define I_AMP_GAIN (27e3f / 12e3f)
#define RC_GAIN    0.9995f       /* 1k/10n at 500 Hz */

#define V_AMP_A    (R_VP_REF / (R_VP_IN + R_VP_REF) * (1.0f + R_VN_FB / R_VN_IN))
#define V_RATIO    (V_AMP_A * R_DIV_BOT / (R_DIV_TOP + R_DIV_BOT) * RC_GAIN) /* V per V */
#define I_RATIO    (R_SHUNT * I_AMP_GAIN * RC_GAIN)                          /* V per A */

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
