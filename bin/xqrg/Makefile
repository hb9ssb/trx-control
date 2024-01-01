SRCS=		xqrg.c \
		qrg.c \
		luaxqrg.c \
		buffer.c \
		luajson.c

OBJS=		${SRCS:.c=.o}

MANDIR?=	/usr/share/man
BINDIR?=	/usr/bin
XQRGDIR?=	/usr/share/xqrg
APP_DEFAULTS?=	/etc/X11/app-defaults

CFLAGS+=	-I../../lib/libtrx-control -I../../external/mit/lua/src \
		-D_GNU_SOURCE
LDFLAGS+=	-L../../lib/libtrx-control -ltrx-control \
		-L../../lib/liblua -llua -lm -lXm -lXt -lX11 -lXm
VPATH=		../../external/mit/luajson ../../lib/libtrx-control

build:		xqrg

clean:
		rm -f xqrg *.o

.PHONY: install xqrg.1 XQRG xqrg.lua
install:	xqrg xqrg.1 XQRG xqrg.lua
		install -d $(DESTDIR)$(BINDIR)
		install -m 755 xqrg $(DESTDIR)$(BINDIR)/xqrg

xqrg:		${OBJS}
		cc ${CFLAGS} -o xqrg ${OBJS} ${LDFLAGS} ${LDADD}

xqrg.1:
		@install -D -m 644 $@ $(DESTDIR)$(MANDIR)/man1/$@
		@gzip -f $(DESTDIR)$(MANDIR)/man1/$@

xqrg.lua:
		@install -D -m 644 $@ $(DESTDIR)$(XQRGDIR)/$@

XQRG:
		@install -D -m 644 $@ $(DESTDIR)$(APP_DEFAULTS)/$@

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<

# Dependencies
luaxqrg.o:	luaxqrg.c xqrg.h trx-control.h

qrg.o:		qrg.c xqrg.h

xqrg.o:		xqrg.c xqrg.h trx-control.h
