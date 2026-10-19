/*
 * format.h
 *
 * Created: 19/10/2026 1:45:08 pm
 *  Author: thoma
 */ 


#ifndef FORMAT_H_
#define FORMAT_H_


#include <stdint.h>

// Number formatting shared by the OLED, 7-segment display and UART.
// The text functions write characters at p and return a pointer just past
// the last one. They do NOT add a '\0'; the caller does that at the end.

// Plain decimal integer, with '-' if negative.  -2625 -> "-2625"
char *fmt_int(char *p, int32_t v);

// A milli-unit value (mV, mA, mW) as units with 4 significant digits and a
// unit letter, at most 7 characters:
//   10618 -> "10.62V"    405 -> "0.405A"    -2625 -> "-2.625W"
char *fmt_milli(char *p, int32_t m, char unit);

// Same 4 significant digits, for the 7-segment display.
// Returns the 4-digit number (0..9999) and sets *dp to the digit that gets
// the decimal point (0 = "d.ddd", 1 = "dd.dd", 2 = "ddd.d", 3 = "dddd.").
// Pass it straight to seperate_and_load_characters(n, dp).
uint16_t fmt_4digit(uint32_t m, uint8_t *dp);




#endif /* FORMAT_H_ */