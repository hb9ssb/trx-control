SRCS=		trxctl.c \
		luatrxctl.c \
		buffer.c \
		luajson.c

OBJS=		${SRCS:.c=.o}

MANDIR?=	/usr/share/man
BINDIR?=	/usr/bin
TRXCTLDIR?=	/usr/share/trxctl

CFLAGS+=	-I../../lib/libtrx-control -I../../external/mit/lua/src \
		-D_GNU_SOURCE
LDFLAGS+=	-L../../lib/libtrx-control -ltrx-control \
		-L../../lib/liblua -llua -lm -lreadline
VPATH=		../../external/mit/luajson \
		../../lib/libtrx-control

build:		trxctl

clean:
		rm -f trxctl *.o

.PHONY: install trxctl.1 trxctl.lua
install:	trxctl trxctl.1 trxctl.lua
		install -d $(DESTDIR)$(BINDIR)
		install -m 755 trxctl $(DESTDIR)$(BINDIR)/trxctl

trxctl:		${OBJS}
		cc ${CFLAGS} -o trxctl ${OBJS} ${LDFLAGS} ${LDADD}

trxctl.1:
		@install -D -m 644 $@ $(DESTDIR)$(MANDIR)/man1/$@
		@gzip -f $(DESTDIR)$(MANDIR)/man1/$@

trxctl.lua:
		@install -D -m 644 $@ $(DESTDIR)$(TRXCTLDIR)/$@

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<

# Dependencies

luatrxctl.o:	luatrxctl.c trx-control.h

trxctl.o:	trxctl.c trx-control.h
