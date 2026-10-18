/*
 * display.h  -  4-digit 7-segment display through the shift register (P4 header)
 *
 * Driver from Lab 6 Part 2 (tsha374), extended with unit letters, a value
 * formatter and a 2.5 ms system tick from the same Timer0 interrupt.
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdint.h>

/* segment patterns for unit letters (bit order: dp g f e d c b a) */
#define SEG_U     0x3E   /* voltage  */
#define SEG_A     0x77   /* current  */
#define SEG_P     0x73   /* power    */
#define SEG_E     0x79   /* energy   */
#define SEG_F     0x71   /* PF       */
#define SEG_DASH  0x40
#define SEG_BLANK 0x00

#define TICK_MS_X10 25   /* one tick = 2.5 ms */

void init_display(void);
void seperate_and_load_characters(uint16_t number, uint8_t decimal_pos);   /* Lab 6 */
void send_next_character_to_display(void);                                 /* Lab 6 */

/* Show a value given in milli-units with 3 significant digits plus a unit letter
 * in the right-most digit, e.g. (12984, SEG_U) -> "12.9U", (264, SEG_A) -> "0.26A". */
void display_value(uint32_t milli, uint8_t unit_pattern);

/* Show 4 raw segment patterns (left to right). */
void display_raw(const uint8_t p[4]);

/* Fill the 4 digit patterns without showing them (testable on a PC). */
void display_format(uint32_t milli, uint8_t unit_pattern, uint8_t out[4]);

/* Timer0: multiplexes one digit every 2.5 ms (100 Hz refresh) and counts ticks. */
void display_timer_init(void);
uint32_t ticks(void);    /* 2.5 ms ticks since power-up */

#endif /* DISPLAY_H_ */
