SRCS=		bluecat.c

OBJS=		${SRCS:.c=.o}

MANDIR?=	/usr/share/man
BINDIR?=	/usr/bin

CFLAGS+=	-D_GNU_SOURCE -DVERSION=\"${VERSION}\"
LDFLAGS+=	-lbluetooth

build:		bluecat

clean:
		rm -f bluecat *.o

.PHONY: install bluecat.1
install:	bluecat bluecat.1
		install -d $(DESTDIR)$(BINDIR)
		install -m 755 bluecat $(DESTDIR)$(BINDIR)/bluecat

bluecat:	${OBJS}
		cc ${CFLAGS} -o $@ ${OBJS} ${LDFLAGS} ${LDADD}

bluecat.1:
		@install -D -m 644 $@ $(DESTDIR)$(MANDIR)/man1/$@
		@gzip -f $(DESTDIR)$(MANDIR)/man1/$@

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
