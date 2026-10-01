/*
 * I2C screen code.c
 *
 * Created: 28/09/2026 1:18:30 pm
 * Author : thoma
 */ 

#include "common.h"   // defines F_CPU; must come before <util/delay.h>
#include <avr/io.h>
#include <util/delay.h>
 
#include "i2c.h"
#include "sh1106.h"
#include "graph.h"
 
int main(void)
{
    i2c_init();
    sh1106_init();
 
    while (1) {
        graph_render();
 
        for (uint8_t i = 0; i < 10; i++) {
            _delay_ms(100);
        }
    }
}


