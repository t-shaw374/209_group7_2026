/*
 * timeout.c
 *
 * Created: 18/10/2026 8:27:26 pm
 *  Author: thoma
 */ 
#include "common.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include "timeout.h"

#if F_CPU != 2000000UL
#error "timeout.c: ms-to-ticks conversion assumes F_CPU = 2 MHz"
#endif

#define T0_CS  ((1 << CS02) | (1 << CS00))   // prescaler 1024 -> 512 us per tick

static volatile uint8_t expired;

void timeout_init(void)
{
	TCCR0B = 0;                                  // stopped
	TCCR0A = 0;                                  // normal mode, OC0A/OC0B pins off
	TIMSK0 = 0;
	expired = 0;
}

void timeout_start(uint8_t ms)
{
	if (ms == 0) {
		ms = 1;
	}
	if (ms > TIMEOUT_MAX_MS) {
		ms = TIMEOUT_MAX_MS;
	}

	// ticks = ms * 1953.125 / 1000 = ms * 125 / 64, rounded up.
	// +1 because the prescaler isn't reset, so the first tick can come almost
	// straight away; this way the timeout is never short. 130 ms -> 255 ticks.
	uint8_t ticks = (uint8_t)((((uint16_t)ms * 125 + 63) / 64) + 1);

	TCCR0B = 0;                                  // stop while setting up
	TIMSK0 = 0;
	TCNT0  = 0;
	OCR0A  = ticks;                              // finish time
	expired = 0;
	TIFR0  = (1 << OCF0A);                       // clear stale flag (write 1)
	TIMSK0 = (1 << OCIE0A);
	TCCR0B = T0_CS;                              // go
}

void timeout_stop(void)
{
	TCCR0B = 0;
	TIMSK0 = 0;
}

uint8_t timeout_expired(void)
{
	return expired;
}

// Finish time reached. Runs at most once per timeout_start(), and only when
// something has already gone wrong, so it never disturbs normal ADC timing.
// ISR_NOBLOCK anyway, so the zero-crossing interrupt is never held up by it.
ISR(TIMER0_COMPA_vect, ISR_NOBLOCK)
{
	TCCR0B  = 0;                                 // one-shot: stop the timer
	TIMSK0  = 0;
	expired = 1;
}