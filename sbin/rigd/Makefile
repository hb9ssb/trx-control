SRCS=		rigd.c rig-control.c client-handler.c luarig.c
OBJS=		${SRCS:.c=.o}


MANDIR?=	/usr/share/man
SBINDIR?=	/usr/sbin

CFLAGS+=	-I../../external/mit/lua/src -pthread
LDFLAGS+=	-L../../lib/liblua -llua -lm

build:		rigd

clean:
		rm -f rigd *.o

.PHONY: install rigd.8
install:	rigd rigd.8
		install -m 755 rigd $(DESTDIR)$(SBINDIR)

rigd:		${OBJS}
		cc ${CFLAGS} -o rigd ${OBJS} ${LDFLAGS} ${LDADD}

rigd.8:
		@install -D -m 644 $@ $(DESTDIR)$(MANDIR)/man8/$@
		@gzip -f $(DESTDIR)$(MANDIR)/man8/$@

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<

rigd.o:		rigd.c rigd.h

luarig.o:	luarig.c rigd.h
