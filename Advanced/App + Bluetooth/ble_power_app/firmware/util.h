/*
 * util.h  -  small text helpers for the command parser (PC-testable)
 */

#ifndef UTIL_H_
#define UTIL_H_

#include <stdint.h>

/* "13", "13.0", "0.264" -> milli-units (13000, 13000, 264). Returns 1 if valid. */
uint8_t parse_milli(const char *s, uint32_t *out);

/* In-place upper-case. */
void str_upper(char *s);

/* If line starts with "NAME," returns pointer to the argument after the comma,
 * if line equals "NAME" returns pointer to "" ; otherwise 0. */
const char *match_cmd(const char *line, const char *name);

/* Writes milli-units as text with 1-3 decimals into out (no terminator added),
 * e.g. (3405, 2) -> "3.41". Returns the number of characters (max 11). */
uint8_t fmt_fixed(char *out, uint32_t milli, uint8_t decimals);

#endif /* UTIL_H_ */
