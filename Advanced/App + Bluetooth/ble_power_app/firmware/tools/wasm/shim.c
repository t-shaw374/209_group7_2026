/* shim.c - the few libc pieces doom_core.c needs when built for the browser */
#include <stddef.h>
#include <stdint.h>
#include "scratch.h"

uint16_t scratch_buf[SCRATCH_WORDS];

char *strcpy(char *d, const char *s)
{
	char *r = d;
	while ((*d++ = *s++) != '\0') {
	}
	return r;
}

void *memset(void *p, int v, size_t n)
{
	unsigned char *q = p;
	while (n--) *q++ = (unsigned char)v;
	return p;
}

void *memcpy(void *d, const void *s, size_t n)
{
	unsigned char *a = d;
	const unsigned char *b = s;
	while (n--) *a++ = *b++;
	return d;
}
