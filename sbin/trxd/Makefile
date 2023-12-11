SRCS=		trxd.c \
		dispatcher.c \
		extension.c \
		trx-controller.c \
		gpio-controller.c \
		relay-controller.c \
		socket-handler.c \
		socket-sender.c \
		trx-handler.c \
		trx-poller.c \
		luatrxd.c \
		luatrx-controller.c \
		luatrx.c \
		nmea-handler.c \
		proxy.c \
		luayaml.c \
		buffer.c \
		luajson.c \
		websocket-listener.c \
		websocket-handler.c \
		websocket-sender.c \
		websocket.c \
		base64.c

OBJS=		${SRCS:.c=.o}

MANDIR?=	/usr/share/man
SBINDIR?=	/usr/sbin
TRXDDIR?=	/usr/share/trxd

CFLAGS+=	-I../../lib/libtrx-control -I../../external/mit/lua/src \
		-I../../external/mit/luayaml \
		-I../../external/mit/luajson -pthread -D_GNU_SOURCE \
		-export-dynamic
LDFLAGS+=	-L../../lib/libtrx-control -ltrx-control \
		-L../../lib/liblua -llua -lyaml -lm -lssl -lcrypto
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
dispatcher.o:		Makefile dispatcher.c trxd.h trx-control.h
socket-handler.o:	Makefile socket-handler.c trxd.h trx-control.h
socket-sender.o:	Makefile socket-sender.c trxd.h trx-control.h
websocket-sender.o:	Makefile websocket-sender.c trxd.h trx-control.h \
			websocket.h
websocket.o:		Makefile websocket.c websocket.h
base64.o:		Makefile base64.c base64.h

luatrx.o:	Makefile luatrx.c trxd.h

luatrxd.o:	Makefile luatrxd.c trxd.h trx-control.h

luatrx-controller.o:	Makefile luatrx-controller.c trxd.h trx-control.h

proxy.o:	Makefile proxy.c trx-control.h

extension.o:		Makefile extension.c pathnames.h trxd.h

trx-controller.o:	Makefile trx-controller.c pathnames.h trxd.h

gpio-controller.o:	Makefile gpio-controller.c pathnames.h trxd.h

relay-controller.o:	Makefile relay-controller.c pathnames.h trxd.h

trx-handler.o:	Makefile trx-handler.c trxd.h

nmea-handler.o:	Makefile nmea-handler.c trxd.h

trx-poller.o:	Makefile trx-poller.c trxd.h

trxd.o:		Makefile trxd.c trxd.h trx-control.h

