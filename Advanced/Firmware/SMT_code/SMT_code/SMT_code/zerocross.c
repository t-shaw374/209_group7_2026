/*
 * zerocross.c
 *
 * Created: 17/10/2026 4:42:49 pm
 *  Author: tsha374
 */ 
#include "common.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include "zerocross.h"
 
void zc_init(void)
{
	DDRC   &= ~(1 << PC0);        // PC0 as input (add a pull-up only if your circuit needs it)
	PCMSK1 |=  (1 << PCINT8);     // allow PC0 to trigger the group 1 vector
	PCICR  |=  (1 << PCIE1);      // enable pin-change interrupt group 1 (PC0..PC6)
}
 
ISR(PCINT1_vect)
{
	// Pin-change fires on both edges, so read the pin to pick rising only
	if (PINC & (1 << PC0)) {
		/* TODO: work to do on each zero crossing */
	}
}