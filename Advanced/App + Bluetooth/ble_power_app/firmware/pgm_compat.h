/*
 * pgm_compat.h  -  lets the same code read flash tables on the AVR and on a PC (tests)
 */

#ifndef PGM_COMPAT_H_
#define PGM_COMPAT_H_

#ifdef __AVR__
#include <avr/pgmspace.h>
#else
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t *)(p))
#define pgm_read_word(p) (*(const uint16_t *)(p))
#define pgm_read_dword(p) (*(const uint32_t *)(p))
#define PSTR(s) (s)
#define strcpy_P(d, s) strcpy((d), (s))
#endif

#endif /* PGM_COMPAT_H_ */
