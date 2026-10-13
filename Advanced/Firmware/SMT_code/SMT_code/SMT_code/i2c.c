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

#define I2C_SCL_HZ 100000UL

void i2c_init(void)
{
	TWSR0 = 0x00; // pre-scaler = 1
	TWBR0 = ((F_CPU / I2C_SCL_HZ) - 16) / 2; // TWBR = 2
	TWCR0 = (1 << TWEN); // Enable TWI
}


uint8_t i2c_start(uint8_t address_with_rw)
{
	TWCR0 = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN); // Setup line for starting transmission
	while (!(TWCR0 & (1 << TWINT))); // Waits until finish
	
	TWDR0 = address_with_rw; // Loads address to be sent
	TWCR0 = (1 << TWINT) | (1 << TWEN); // sends address
	while (!(TWCR0 & (1 << TWINT))); // waits for address to be sent
	
	return (TWSR0 & 0xF8);
}


uint8_t i2c_write(uint8_t data)
{
	TWDR0 = data; // Load data to be sent
	TWCR0 = (1 << TWINT) | (1 << TWEN); // start transmission
	while (!(TWCR0 & (1 << TWINT))); // Waits until finished
	
	return (TWSR0 & 0xF8);
}

void i2c_stop(void)
{
	TWCR0 = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); // Generate stop condition on line
}
