/*
 * Copyright (C) 2025 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick.
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

/* Process YAML anchors */

#ifndef __ANCHOR_H__
#define __ANCHOR_H__

typedef struct anchor_s {
	yaml_char_t	*anchor;
	int		 ref;
	struct anchor_s	*next;
} anchor_t;

extern anchor_t *anchor_init(void);
extern int anchor_set(lua_State *, anchor_t *, yaml_char_t *, int);
extern int anchor_get(anchor_t *, yaml_char_t *);
extern void anchor_free(lua_State *, anchor_t *);
extern void anchor_dump(anchor_t *);

#endif /* __ANCHOR_H__ */
