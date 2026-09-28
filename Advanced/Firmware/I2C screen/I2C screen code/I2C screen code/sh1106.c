/*
 * sh1106.c
 *
 * Created: 28/09/2026 2:39:13 pm
 *  Author: thoma
 */ 

#include "common.h"
#include "sh1106.h"
#include "i2c.h"
#include <util/delay.h>

#define CTRL_COMMAND 0x00
#define CTRL_DATA    0x40

// SH1106 has 132 columns of internal RAM but only the middle 128
// are actually shown on the panel, so every column write needs to
// start 2 columns in, or the image shifts / clips on one edge.
#define SH1106_COLUMN_OFFSET 2

// This file has zero .data/.bss footprint: every command byte below
// lives in flash as an immediate, not in a RAM table.

void sh1106_command(uint8_t cmd)
{
	i2c_start((SH1106_ADDR << 1) | 0);
	i2c_write(CTRL_COMMAND);
	i2c_write(cmd);
	i2c_stop();
}

void sh1106_init(void)
{
	_delay_ms(100); // let the panel power up

	sh1106_command(0xAE); // display off

	sh1106_command(0xD5); // clock divide
	sh1106_command(0x80);

	sh1106_command(0xA8); // multiplex ratio
	sh1106_command(0x3F); // 64 rows - 1

	sh1106_command(0xD3); // display offset
	sh1106_command(0x00);

	sh1106_command(0x40); // start line = 0

	sh1106_command(0xAD); // DC-DC / charge pump control (SH1106-specific)
	sh1106_command(0x8B); // enable

	sh1106_command(0xA1); // segment remap (mirror X)
	sh1106_command(0xC8); // COM scan direction (mirror Y)

	sh1106_command(0xDA); // COM pins config
	sh1106_command(0x12);

	sh1106_command(0x81); // contrast
	sh1106_command(0x80);

	sh1106_command(0xD9); // pre-charge period
	sh1106_command(0x1F);

	sh1106_command(0xDB); // VCOM deselect level
	sh1106_command(0x40);

	sh1106_command(0xA4); // resume RAM content display
	sh1106_command(0xA6); // normal (non-inverted) display

	sh1106_command(0xAF); // display on
}

void sh1106_set_page(uint8_t page)
{
	sh1106_command(0xB0 | (page & 0x07)); // set page address 0-7
	sh1106_command(SH1106_COLUMN_OFFSET & 0x0F);        // lower column nibble
	sh1106_command(0x10 | (SH1106_COLUMN_OFFSET >> 4)); // upper column nibble
}

void sh1106_write_page_data(const uint8_t *data, uint8_t len)
{
	i2c_start((SH1106_ADDR << 1) | 0);
	i2c_write(CTRL_DATA);
	for (uint8_t i = 0; i < len; i++) {
		i2c_write(data[i]);
	}
	i2c_stop();
}