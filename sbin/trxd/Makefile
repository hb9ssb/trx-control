SRCS=		trxd.c trx-control.c client-handler.c luatrx.c
OBJS=		${SRCS:.c=.o}


MANDIR?=	/usr/share/man
SBINDIR?=	/usr/sbin

CFLAGS+=	-I../../external/mit/lua/src -pthread
LDFLAGS+=	-L../../lib/liblua -llua -lm

build:		trxd

clean:
		rm -f trxd *.o

.PHONY: install trxd.8
install:	trxd trxd.8
		install -m 755 trxd $(DESTDIR)$(SBINDIR)

trxd:		${OBJS}
		cc ${CFLAGS} -o trxd ${OBJS} ${LDFLAGS} ${LDADD}

trxd.8:
		@install -D -m 644 $@ $(DESTDIR)$(MANDIR)/man8/$@
		@gzip -f $(DESTDIR)$(MANDIR)/man8/$@

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<

trxd.o:		trxd.c trxd.h

luatrx.o:	luatrx.c trxd.h
