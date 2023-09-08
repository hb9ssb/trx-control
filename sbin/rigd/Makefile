SRCS=		rigd.c
OBJS=		${SRCS:.c=.o}


MANDIR?=	/usr/share/man
SBINDIR?=	/usr/sbin

build:		rigd

clean:
		rm -f rigd *.o

.PHONY: install rigd.8
install:	rigd rigd.8

rigd:		${OBJS}
		cc ${CFLAGS} ${LDFLAGS} -o rigd ${OBJS} ${LDADD}

rigd.8:
		@install -D -m 644 $@ $(DESTDIR)$(MANDIR)/man8/$@
		@gzip -f $(DESTDIR)$(MANDIR)/man8/$@

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<

