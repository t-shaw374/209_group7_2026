/*
 * I2C screen code.c
 *
 * Created: 28/09/2026 1:18:30 pm
 * Author : thoma
 */ 

#include "common.h"
#include <avr/io.h>

#include "i2c.h"

#define TW_MT_SLA_ACK 0x18
#define TARGET_ADDR   0x3C   // change to whatever device you're testing against

int main(void)
{
	i2c_init();
	
	uint8_t status = i2c_start((TARGET_ADDR << 1) | 0); // address + write
	
	if (status == TW_MT_SLA_ACK) {
		i2c_write('A');
		i2c_write('A');
		i2c_write('A');
	}
	
	i2c_stop();
	
	while (1) {
		// done
	}
}


