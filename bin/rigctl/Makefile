SRCS=		rigctl.c
OBJS=		${SRCS:.c=.o}


MANDIR?=	/usr/share/man
BINDIR?=	/usr/bin

build:		rigctl

clean:
		rm -f rigctl *.o

.PHONY: install rigctl.1
install:	rigctl rigctl.1
		install -m 755 rigctl $(DESTDIR)$(BINDIR)

rigctl:		${OBJS}
		cc ${CFLAGS} ${LDFLAGS} -o rigctl ${OBJS} ${LDADD}

rigctl.1:
		@install -D -m 644 $@ $(DESTDIR)$(MANDIR)/man1/$@
		@gzip -f $(DESTDIR)$(MANDIR)/man1/$@

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
