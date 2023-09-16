/*
 * Copyright (C) 2021 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "buffer.h"

int
buf_init(struct buffer *b)
{
        if ((b->data = malloc(BUFFER_SIZE)) == NULL)
                return -1;
        b->capacity = BUFFER_SIZE;
        b->size = 0;
        return 0;
}

static void
buf_resize(struct buffer *b, size_t size)
{
        size_t enlargement = BUFFER_SIZE;

        while (enlargement < size)
                enlargement += BUFFER_SIZE;
        b->capacity += enlargement;
        b->data = realloc(b->data, b->capacity);
}

void
buf_addstring(struct buffer *b, const char *s)
{
        size_t len;

        len = strlen(s);
        if (b->size + len >= b->capacity)
                buf_resize(b, len);
        strcpy(b->data + b->size, s);
        b->size += len;
}

void
buf_addchar(struct buffer *b, char c)
{
        if (b->size + 1 >= b->capacity)
                buf_resize(b,  1);
        *(b->data + b->size++) = c;
}

void
buf_push(struct buffer *b, lua_State *L)
{
        lua_pushlstring(L, b->data, b->size);
}

void
buf_free(struct buffer *b)
{
        free(b->data);
}
