SRCS=		trxctl.c
OBJS=		${SRCS:.c=.o}


MANDIR?=	/usr/share/man
BINDIR?=	/usr/bin

build:		trxctl

clean:
		rm -f trxctl *.o

.PHONY: install trxctl.1
install:	trxctl trxctl.1
		install -m 755 trxctl $(DESTDIR)$(BINDIR)

trxctl:		${OBJS}
		cc ${CFLAGS} ${LDFLAGS} -o trxctl ${OBJS} ${LDADD}

trxctl.1:
		@install -D -m 644 $@ $(DESTDIR)$(MANDIR)/man1/$@
		@gzip -f $(DESTDIR)$(MANDIR)/man1/$@

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
