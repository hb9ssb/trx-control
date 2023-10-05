/*
 * Copyright (c) 2023 Marc Balmer HB9SSB
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

#ifndef __XQRG_H__
#define __XQRG_H__

extern XtAppContext app;	/* The application context */
extern Widget toplevel;		/* The toplevel shell */

#define PATH_XQRG	"/usr/share/xqrg"

typedef struct ConfigRec {
	String	host;
	String	port;
	Boolean	verbose;
} ConfigRec;

#define XtNhost		"host"
#define XtNport		"port"
#define XtNverbose	"verbose"

/* Functions to create a compound area */
Widget create_qrg(Widget);

extern int luaopen_xqrg(lua_State *);

extern void scroll_start(const char *);
extern void scroll_stop(void);
extern void clear_display(void);

extern void qrg_status(char *);
extern void qrg_addchar(char);
extern void qrg_addstring(char *);
extern void qrg_reset(void);
extern void qrg_clrscr(void);
extern void qrg_clreol(void);
extern void qrg_position(int);
extern void qrg_scrollup(void);

#endif /* __XQRG_H__ */

