/*
 * timeout.h
 *
 * Created: 18/10/2026 8:31:27 pm
 *  Author: thoma
 */ 


#ifndef TIMEOUT_H_
#define TIMEOUT_H_


#include <stdint.h>

// One-shot hardware timeout on Timer0.
//
// timeout_start(ms) starts Timer0 (prescaler 1024, 512 us per tick) with a
// compare match at the finish time. If the match is reached, the compare ISR
// stops the timer and sets a flag that timeout_expired() returns.
// timeout_stop() cancels it when the work finished in time, so in normal
// operation the ISR never runs.
//
// The ISR only sets a flag. Code waiting in a loop must check timeout_expired()
// itself and bail out; the ISR can't break a loop on its own.
//
// Longest timeout: 130 ms (one pass of the 8-bit counter).
//
//   timeout_start(125);
//   while (!finished) {
//       if (timeout_expired()) { ...give up... }
//   }
//   timeout_stop();

#define TIMEOUT_MAX_MS  130

// Put Timer0 in a known, stopped state. Call once at startup, before sei().
void timeout_init(void);

// Start (or restart) the timeout, 1..TIMEOUT_MAX_MS ms (clamped).
// Clears any earlier expiry.
void timeout_start(uint8_t ms);

// Cancel the timeout. Safe to call when it isn't running.
void timeout_stop(void);

// 1 if the timeout ran out since the last timeout_start(), else 0.
uint8_t timeout_expired(void);



#endif /* TIMEOUT_H_ */