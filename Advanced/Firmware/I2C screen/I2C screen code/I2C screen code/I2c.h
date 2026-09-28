/*
 * I2c.h
 *
 * Created: 28/09/2026 1:40:13 pm
 *  Author: thoma
 */ 


#ifndef I2C_H_
#define I2C_H_

#include <stdint.h>


// F_CPU must be defined by the project (Project Properties -> Toolchain -> Symbols),
// e.g. F_CPU=16000000UL

void i2c_init(void);
uint8_t i2c_start(uint8_t address_with_rw);
uint8_t i2c_write(uint8_t data);
void i2c_stop(void);




#endif /* I2C_H_ */