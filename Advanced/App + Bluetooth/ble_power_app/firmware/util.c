/*
 * util.c  -  see util.h
 */

#include "util.h"

uint8_t parse_milli(const char *s, uint32_t *out)
{
	uint32_t whole = 0, frac = 0;
	uint8_t  ndig = 0, nfrac = 0, seen_dot = 0;

	if (!s || !*s) return 0;
	for (; *s; s++) {
		if (*s >= '0' && *s <= '9') {
			if (!seen_dot) {
				if (whole > 400000UL) return 0;          /* keep milli * 1000 inside 32 bits */
				whole = whole * 10 + (uint32_t)(*s - '0');
			} else if (nfrac < 3) {
				frac = frac * 10 + (uint32_t)(*s - '0');
				nfrac++;
			}                                          /* extra decimals ignored */
			ndig++;
		} else if (*s == '.' && !seen_dot) {
			seen_dot = 1;
		} else {
			return 0;
		}
	}
	if (ndig == 0) return 0;
	while (nfrac < 3) { frac *= 10; nfrac++; }
	*out = whole * 1000UL + frac;
	return 1;
}

void str_upper(char *s)
{
	for (; *s; s++) {
		if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 'a' + 'A');
	}
}

const char *match_cmd(const char *line, const char *name)
{
	while (*name) {
		if (*line != *name) return 0;
		line++;
		name++;
	}
	if (*line == '\0') return line;
	if (*line == ',')  return line + 1;
	return 0;
}

uint8_t fmt_fixed(char *out, uint32_t milli, uint8_t decimals)
{
	uint32_t div, scale;
	if (decimals == 1)      { div = 100; scale = 10;   }
	else if (decimals == 2) { div = 10;  scale = 100;  }
	else                    { div = 1;   scale = 1000; }

	uint32_t v = (milli + div / 2) / div;
	uint32_t whole = v / scale, frac = v % scale;
	char tmp[10];
	uint8_t n = 0, len = 0;
	do { tmp[n++] = (char)('0' + whole % 10); whole /= 10; } while (whole);
	while (n) out[len++] = tmp[--n];
	out[len++] = '.';
	for (uint32_t p = scale / 10; p > 0; p /= 10) out[len++] = (char)('0' + (frac / p) % 10);
	return len;
}
