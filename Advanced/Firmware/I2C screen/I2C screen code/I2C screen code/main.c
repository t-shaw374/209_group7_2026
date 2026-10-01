/*
 * I2C screen code.c
 *
 * Created: 28/09/2026 1:18:30 pm
 * Author : thoma
 */ 

#include "common.h"   // defines F_CPU; must come before <util/delay.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "i2c.h"
#include "sh1106.h"
#include "font_chars.h"

#define CHAR_WIDTH 6   // 5 pixel-columns of glyph + 1 pixel spacing

static void draw_string(uint8_t *buf, uint8_t x, const char *str)
{
	while (*str) {
		const uint8_t *glyph = get_glyph(*str);
		for (uint8_t col = 0; col < 5; col++) {
			if (x + col < SCREEN_WIDTH) {
				buf[x + col] |= pgm_read_byte(&glyph[col]);
			}
		}
		x += CHAR_WIDTH;
		str++;
	}
}

int main(void)
{
	i2c_init();
	sh1106_init();
	
	uint8_t buf[SCREEN_WIDTH]; // only RAM this uses: 128 bytes, one page at a time
	
	while (1) {
		for (uint8_t page = 0; page < SCREEN_PAGES; page++) {
			for (uint8_t i = 0; i < SCREEN_WIDTH; i++) buf[i] = 0x00;
			
			if (page == 3) {  // roughly vertically centered on a 64px display
				draw_string(buf, 50, "Hello World");
			}
			
			sh1106_set_page(page);
			sh1106_write_page_data(buf, SCREEN_WIDTH);
		}
		
		for (uint8_t i = 0; i < 10; i++) {
			_delay_ms(100);
		}
	}
}

