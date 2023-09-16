SRCS=		trxd.c \
		trx-control.c \
		client-handler.c \
		trx-handler.c \
		luatrxd.c \
		luatrx.c \
		luayaml.c \
		buffer.c \
		luajson.c

OBJS=		${SRCS:.c=.o}

MANDIR?=	/usr/share/man
SBINDIR?=	/usr/sbin
TRXDDIR?=	/usr/share/trxd

CFLAGS+=	-I../../external/mit/lua/src -I../../external/mit/luayaml \
		-I../../external/mit/luajson -pthread -D_GNU_SOURCE
LDFLAGS+=	-L../../lib/liblua -llua -lyaml -lm
VPATH=		../../external/mit/luayaml ../../external/mit/luajson

build:		trxd

clean:
		rm -f trxd *.o

.PHONY: install trxd.8 trx-control.lua
install:	trxd trxd.8 trx-control.lua
		install -m 755 trxd $(DESTDIR)$(SBINDIR)

trxd:		${OBJS}
		cc ${CFLAGS} -o trxd ${OBJS} ${LDFLAGS} ${LDADD}

trxd.8:
		@install -D -m 644 $@ $(DESTDIR)$(MANDIR)/man8/$@
		@gzip -f $(DESTDIR)$(MANDIR)/man8/$@

trx-control.lua:
		@install -D -m 644 $@ $(DESTDIR)$(TRXDDIR)/$@

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<

trxd.o:		trxd.c trxd.h

luatrx.o:	luatrx.c trxd.h
