/*
 * oled.c  -  see oled.h
 *
 * Software (bit-banged) I2C master. Lines are driven open-drain: a 0 is driven
 * low, a 1 is released and pulled up by the pull-up resistors on the OLED module
 * (almost every module has them; if yours doesn't, add 4.7k from SDA and SCL to
 * VCC). Only the DDR bit is toggled, one instruction per edge, so at 2 MHz the
 * clock runs at about 200 kHz, inside the SSD1306's 400 kHz limit.
 */

#include <avr/io.h>
#include "common.h"
#include "oled.h"

#ifndef OLED_ADDR
#define OLED_ADDR 0x3C               /* 7-bit address; some modules use 0x3D */
#endif

#define SDA_BIT PB0
#define SCL_BIT PB1

/* open-drain helpers: PORTB bits stay 0, so output = low, input = released (high) */
#define SDA_LOW()   (DDRB |=  (1 << SDA_BIT))
#define SDA_HIGH()  (DDRB &= ~(1 << SDA_BIT))
#define SCL_LOW()   (DDRB |=  (1 << SCL_BIT))
#define SCL_HIGH()  (DDRB &= ~(1 << SCL_BIT))
#define SDA_READ()  (PINB & (1 << SDA_BIT))

static uint8_t present = 0;
static uint8_t fast = 0;              /* CPU at 16 MHz (DOOM turbo): stretch the clock */

/* I2C fast mode needs SCL high >= 0.6 us and low >= 1.3 us. At 2 MHz the code
 * itself is slow enough; at 16 MHz these pauses keep the clock near 400 kHz. */
#define HOLD_HIGH() do { if (fast) __builtin_avr_delay_cycles(8);  } while (0)
#define HOLD_LOW()  do { if (fast) __builtin_avr_delay_cycles(16); } while (0)

void oled_set_fast(uint8_t on)
{
	fast = on;
}

static void i2c_start(void)
{
	SDA_HIGH(); SCL_HIGH(); HOLD_LOW();
	SDA_LOW();  HOLD_HIGH();
	SCL_LOW();  HOLD_LOW();
}

static void i2c_stop(void)
{
	SDA_LOW();  HOLD_LOW();
	SCL_HIGH(); HOLD_HIGH();
	SDA_HIGH(); HOLD_LOW();
}

/* returns 1 if the device acknowledged */
static uint8_t i2c_write(uint8_t b)
{
	for (uint8_t m = 0x80; m; m >>= 1) {
		if (b & m) SDA_HIGH(); else SDA_LOW();
		HOLD_LOW();
		SCL_HIGH();
		HOLD_HIGH();
		SCL_LOW();
	}
	SDA_HIGH();                     /* release for ACK */
	HOLD_LOW();
	SCL_HIGH();
	HOLD_HIGH();
	uint8_t ack = !SDA_READ();
	SCL_LOW();
	return ack;
}

static void cmd_list(const uint8_t *c, uint8_t n)
{
	i2c_start();
	i2c_write(OLED_ADDR << 1);
	i2c_write(0x00);                /* control byte: commands follow */
	while (n--) i2c_write(*c++);
	i2c_stop();
}

uint8_t oled_init(void)
{
	PORTB &= ~((1 << SDA_BIT) | (1 << SCL_BIT));   /* outputs drive low when enabled */
	SDA_HIGH(); SCL_HIGH();

	i2c_start();
	present = i2c_write(OLED_ADDR << 1);
	i2c_stop();
	if (!present) return 0;

	static const uint8_t init[] = {
		0xAE,             /* display off */
		0xD5, 0x80,       /* clock divide */
		0xA8, 0x1F,       /* multiplex: 32 rows */
		0xD3, 0x00,       /* no display offset */
		0x40,             /* start line 0 */
		0x8D, 0x14,       /* charge pump on (module runs from 5 V / 3.3 V) */
		0x20, 0x01,       /* vertical addressing mode */
		0xA1,             /* column 127 -> SEG0 (flip so it isn't mirrored) */
		0xC8,             /* scan COM from the bottom (flip vertically) */
		0xDA, 0x02,       /* COM pins for 128 x 32 */
		0x81, 0x8F,       /* contrast */
		0xD9, 0xF1,       /* pre-charge */
		0xDB, 0x40,       /* VCOMH */
		0xA4,             /* show RAM contents */
		0xA6,             /* normal (not inverted) */
		0xAF              /* display on */
	};
	cmd_list(init, sizeof init);
	oled_clear();
	return 1;
}

uint8_t oled_present(void)
{
	return present;
}

void oled_frame_begin(void)
{
	if (!present) return;
	static const uint8_t win[] = { 0x21, 0, OLED_W - 1, 0x22, 0, OLED_H / 8 - 1 };
	cmd_list(win, sizeof win);
	i2c_start();
	i2c_write(OLED_ADDR << 1);
	i2c_write(0x40);                /* control byte: display data follows */
}

void oled_column(uint32_t rows)
{
	if (!present) return;
	i2c_write((uint8_t)rows);
	i2c_write((uint8_t)(rows >> 8));
	i2c_write((uint8_t)(rows >> 16));
	i2c_write((uint8_t)(rows >> 24));
}

void oled_frame_end(void)
{
	if (!present) return;
	i2c_stop();
}

void oled_clear(void)
{
	oled_frame_begin();
	for (uint8_t x = 0; x < OLED_W; x++) oled_column(0);
	oled_frame_end();
}

#ifdef OLED_BENCH
void oled_force_present(void) { present = 1; }
void oled_force_absent(void) { present = 0; }
#endif
