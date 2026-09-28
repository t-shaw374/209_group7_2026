/*
 * I2c.c
 *
 * Created: 28/09/2026 1:39:56 pm
 *  Author: thoma
 */ 

#include "common.h"
#include "I2c.h"
#include <avr/io.h>
#include <stdint.h>

#define I2C_SCL_HZ 100000UL

void i2c_init(void)
{
	TWSR = 0x00; // pre-scaler = 1
	TWBR = ((F_CPU / I2C_SCL_HZ) - 16) / 2; // TWBR = 2
	TWCR = (1 << TWEN); // Enable TWI
}


uint8_t i2c_start(uint8_t address_with_rw)
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN); // Setup line for starting transmission
	while (!(TWCR & (1 << TWINT))); // Waits until finish
	
	TWDR = address_with_rw; // Loads address to be sent
	TWCR = (1 << TWINT) | (1 << TWEN); // sends address
	while (!(TWCR & (1 << TWINT))); // waits for address to be sent
	
	return (TWSR & 0xF8);
}


uint8_t i2c_write(uint8_t data)
{
	TWDR = data; // Load data to be sent
	TWCR = (1 << TWINT) | (1 << TWEN); // start transmission
	while (!(TWCR & (1 << TWINT))); // Waits until finished
	
	return (TWSR & 0xF8);
}

void i2c_stop(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); // Generate stop condition on line
}
