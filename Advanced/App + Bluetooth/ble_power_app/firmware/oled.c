/*
 * oled.c  -  see oled.h
 *
 * SMT board: the OLED header P4 is on the ATmega's hardware I2C (TWI0) pins,
 * PC4 = SDA and PC5 = SCL, with pull-up resistors R14/R15 on the board.
 *
 * Clock:  SCL = F_CPU / (16 + 2 * TWBR)   (prescaler 1)
 *   2 MHz,  TWBR = 2  -> 100 kHz   (normal mode: ~46 ms per frame, once a second)
 *   16 MHz, TWBR = 12 -> 400 kHz   (DOOM turbo: ~12 ms per frame)
 * Every wait has a timeout, so a missing or stuck display can never hang the meter.
 */

#include <avr/io.h>
#include "common.h"
#include "oled.h"

#ifndef OLED_ADDR
#define OLED_ADDR 0x3C               /* 7-bit address; some modules use 0x3D */
#endif

/* ATmega328PB has two TWIs and names the first one TWBR0, TWCR0 ...; 328P has no suffix */
#if defined(TWCR0)
#define TW_BR TWBR0
#define TW_SR TWSR0
#define TW_CR TWCR0
#define TW_DR TWDR0
#else
#define TW_BR TWBR
#define TW_SR TWSR
#define TW_CR TWCR
#define TW_DR TWDR
#endif

#define TWBR_SLOW 2                  /* 100 kHz at 2 MHz  */
#define TWBR_FAST 12                 /* 400 kHz at 16 MHz */

static uint8_t present = 0;

void oled_set_fast(uint8_t on)
{
	TW_BR = on ? TWBR_FAST : TWBR_SLOW;
}

/* wait for the current TWI step; on timeout the bus is stuck, so stop using the
 * display (otherwise every byte of every frame would wait for the timeout) */
static uint8_t tw_wait(void)
{
	for (uint16_t n = 0; n < 4000; n++) {
		if (TW_CR & (1 << TWINT)) return 1;
	}
	present = 0;
	return 0;
}

static uint8_t i2c_start(void)
{
	TW_CR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	if (!tw_wait()) return 0;
	uint8_t st = TW_SR & 0xF8;
	return st == 0x08 || st == 0x10;             /* START / repeated START sent */
}

static void i2c_stop(void)
{
	TW_CR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
	for (uint16_t n = 0; n < 4000 && (TW_CR & (1 << TWSTO)); n++) { ; }
}

/* returns 1 if the device acknowledged */
static uint8_t i2c_write(uint8_t b)
{
	TW_DR = b;
	TW_CR = (1 << TWINT) | (1 << TWEN);
	if (!tw_wait()) return 0;
	uint8_t st = TW_SR & 0xF8;
	return st == 0x18 || st == 0x28;             /* SLA+W ACK / data ACK */
}

static void cmd_list(const uint8_t *c, uint8_t n)
{
	if (!i2c_start()) { i2c_stop(); return; }
	i2c_write(OLED_ADDR << 1);
	i2c_write(0x00);                             /* control byte: commands follow */
	while (n--) i2c_write(*c++);
	i2c_stop();
}

uint8_t oled_init(void)
{
	TW_SR = 0;                                   /* prescaler 1 */
	TW_BR = TWBR_SLOW;
	TW_CR = (1 << TWEN);

	present = 1;
	present = i2c_start() && i2c_write(OLED_ADDR << 1) && present;
	i2c_stop();
	if (!present) return 0;

	static const uint8_t init[] = {
		0xAE,             /* display off */
		0xD5, 0x80,       /* clock divide */
		0xA8, 0x1F,       /* multiplex: 32 rows */
		0xD3, 0x00,       /* no display offset */
		0x40,             /* start line 0 */
		0x8D, 0x14,       /* charge pump on */
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
	if (!i2c_start()) return;
	i2c_write(OLED_ADDR << 1);
	i2c_write(0x40);                             /* control byte: display data follows */
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
