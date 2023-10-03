SRCS=		trxd.c \
		trx-controller.c \
		client-handler.c \
		trx-handler.c \
		trx-poller.c \
		luatrxd.c \
		luatrx.c \
		luayaml.c \
		buffer.c \
		luajson.c

OBJS=		${SRCS:.c=.o}

MANDIR?=	/usr/share/man
SBINDIR?=	/usr/sbin
TRXDDIR?=	/usr/share/trxd

CFLAGS+=	-I../../lib/libtrx-control -I../../external/mit/lua/src \
		-I../../external/mit/luayaml \
		-I../../external/mit/luajson -pthread -D_GNU_SOURCE
LDFLAGS+=	-L../../lib/libtrx-control -ltrx-control \
		-L../../lib/liblua -llua -lyaml -lm
VPATH=		../../external/mit/luayaml ../../external/mit/luajson \
		../../lib/libtrx-control

build:		trxd

clean:
		rm -f trxd *.o

.PHONY: install trxd.8 trx-controller.lua trxd.yaml
install:	trxd trxd.8 trx-controller.lua trxd.yaml
		install -m 755 trxd $(DESTDIR)$(SBINDIR)

trxd:		${OBJS}
		cc ${CFLAGS} -o trxd ${OBJS} ${LDFLAGS} ${LDADD}

trxd.8:
		@install -D -m 644 $@ $(DESTDIR)$(MANDIR)/man8/$@
		@gzip -f $(DESTDIR)$(MANDIR)/man8/$@

trx-controller.lua:
		@install -D -m 644 $@ $(DESTDIR)$(TRXDDIR)/$@

trxd.yaml:
		@install -D -m 644 $@ $(DESTDIR)$(TRXDDIR)/$@

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<

# Dependencies
client-handler.o:	client-handler.c trxd.h trx-control.h

luatrx.o:	luatrx.c trxd.h

luatrxd.o:	luatrxd.c trxd.h trx-control.h

trx-controller.o:	trx-controller.c pathnames.h trxd.h

trx-handler.o:	trx-handler.c trxd.h

trx-poller.o:	trx-poller.c trxd.h

trxd.o:		trxd.c trxd.h trx-control.h

