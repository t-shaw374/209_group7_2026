/*
 * oled.h  -  128 x 32 SSD1306 OLED on the hardware I2C pins (header P4)
 *
 * P4: SDA = PC4 (TWISDA), SCL = PC5 (TWISCL), 5 V, GND. Pull-ups R14/R15 are on
 * the board.
 *
 * Frames are sent column by column (vertical addressing): for each of the 128
 * columns, 4 bytes, top page first; bit 0 of each byte is the top row of that page.
 * That means a whole picture never has to be held in RAM.
 */

#ifndef OLED_H_
#define OLED_H_

#include <stdint.h>

#define OLED_W 128
#define OLED_H 32

/* Returns 1 if a display answered at OLED_ADDR (otherwise all calls do nothing). */
uint8_t oled_init(void);
uint8_t oled_present(void);

void oled_frame_begin(void);          /* then exactly OLED_W calls of oled_column() */
void oled_column(uint32_t rows);      /* bit r = pixel in row r (0 = top) */
void oled_frame_end(void);

void oled_clear(void);

/* 1 while the CPU runs at 16 MHz (DOOM turbo) so the I2C clock stays legal. */
void oled_set_fast(uint8_t on);

#endif /* OLED_H_ */
