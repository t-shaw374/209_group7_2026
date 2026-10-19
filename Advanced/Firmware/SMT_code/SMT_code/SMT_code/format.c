/*
 * format.c
 *
 * Created: 19/10/2026 1:44:58 pm
 *  Author: thoma
 */ 
#include "format.h"

char *fmt_int(char *p, int32_t v)
{
	uint32_t u;
	if (v < 0) {
		*p++ = '-';
		u = (uint32_t)(-(v + 1)) + 1;            // works for INT32_MIN too
		} else {
		u = (uint32_t)v;
	}

	char tmp[10];
	uint8_t n = 0;
	do {
		tmp[n++] = '0' + (u % 10);
		u /= 10;
	} while (u);
	while (n) {
		*p++ = tmp[--n];
	}
	return p;
}

uint16_t fmt_4digit(uint32_t m, uint8_t *dp)
{
	// m is in thousandths. With dp = 0 the 4 digits are d.ddd, so n = m.
	// Each step right divides by 10 and moves the point one digit right.
	uint32_t div = 1;
	uint8_t  d   = 0;
	while (d < 3 && (m + div / 2) / div >= 10000) {
		div *= 10;
		d++;
	}
	uint32_t n = (m + div / 2) / div;
	if (n > 9999) {
		n = 9999;                                // over 9999 units: clamp
	}
	*dp = d;
	return (uint16_t)n;
}

char *fmt_milli(char *p, int32_t m, char unit)
{
	uint32_t u;
	if (m < 0) {
		*p++ = '-';
		u = (uint32_t)(-(m + 1)) + 1;
		} else {
		u = (uint32_t)m;
	}

	uint8_t  dp;
	uint16_t n = fmt_4digit(u, &dp);

	char d[4];
	d[0] = '0' + (n / 1000) % 10;
	d[1] = '0' + (n / 100) % 10;
	d[2] = '0' + (n / 10) % 10;
	d[3] = '0' + n % 10;

	// Integer part is d[0..dp]; drop its leading zeros but keep the last one
	uint8_t i = 0;
	while (i < dp && d[i] == '0') {
		i++;
	}
	for (; i <= dp; i++) {
		*p++ = d[i];
	}
	if (dp < 3) {
		*p++ = '.';
		for (i = dp + 1; i < 4; i++) {
			*p++ = d[i];
		}
	}
	*p++ = unit;
	return p;
}