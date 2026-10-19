/*
 * i2c.c
 *
 * Created: 14/10/2026 12:27:02 pm
 *  Author: tsha374
 */ 
#include "common.h"
#include "i2c.h"
#include <avr/io.h>
#include <stdint.h>

#define I2C_SCL_HZ 50000UL
#define I2C_TIMEOUT  20000U      // loop counts, roughly tens of ms at 2 MHz


void i2c_init(void)
{
	TWSR0 = 0x00;                              // prescaler = 1
	TWBR0 = ((F_CPU / I2C_SCL_HZ) - 16) / 2;   // (40 - 16) / 2 = 12
	TWCR0 = (1 << TWEN);                       // enable TWI
}



static uint8_t i2c_wait(void)
{
	uint16_t t = I2C_TIMEOUT;
	while (!(TWCR0 & (1 << TWINT))) {
		if (--t == 0) {
			TWCR0 = 0;                    // drop out of the stuck state
			TWCR0 = (1 << TWEN);          // re-enable TWI
			return 0;
		}
	}
	return 1;
}

uint8_t i2c_start(uint8_t address_with_rw)
{
	TWCR0 = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	if (!i2c_wait()) return 0xFF;

	TWDR0 = address_with_rw;
	TWCR0 = (1 << TWINT) | (1 << TWEN);
	if (!i2c_wait()) return 0xFF;

	return (TWSR0 & 0xF8);
}

uint8_t i2c_write(uint8_t data)
{
	TWDR0 = data;
	TWCR0 = (1 << TWINT) | (1 << TWEN);
	if (!i2c_wait()) return 0xFF;

	return (TWSR0 & 0xF8);
}

void i2c_stop(void)
{
	TWCR0 = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
	uint16_t t = I2C_TIMEOUT;
	while ((TWCR0 & (1 << TWSTO)) && --t);   // wait for stop to finish
}