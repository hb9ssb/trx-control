/*
 * Copyright (c) 2014 - 2024 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* base64 encoding */

/*
 * Based on:
 *
 * lbase64.c
 * base64 encoding and decoding for Lua 5.1
 * Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br>
 * 23 Mar 2010 22:22:38
 * This code is hereby placed in the public domain.
 */

#include <stdlib.h>
#include <string.h>

#include "base64.h"

static const char code[]=
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void
encode(char *b, unsigned int c1, unsigned int c2, unsigned int c3, int n)
{
	unsigned long tuple = c3 + 256UL * (c2 + 256UL * c1);
	int i;
	char s[5];

	for (i = 0; i < 4; i++) {
		s[3 - i] = code[tuple % 64];
		tuple /= 64;
	}
	for (i = n + 1; i < 4; i++)
		s[i] = '=';
	s[4] = '\0';
	strcat(b, s);
}

char *
base64(unsigned char *s, size_t l)
{
	char *b;
	int n;

	b = calloc(l * 2, 1);
	if (b) {
		for (n = l / 3; n--; s += 3)
			encode(b, s[0], s[1], s[2], 3);
		switch (l % 3) {
		case 1:
			encode(b, s[0], 0, 0, 1);
			break;
		case 2:
			encode(b, s[0], s[1], 0, 2);
			break;
		}
	}
	return b;
}
