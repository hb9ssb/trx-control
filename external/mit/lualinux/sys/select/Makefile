SRCS=		luaselect.c

VPATH=		../../../../mit/lua/src

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH} -D_GNU_SOURCE

all:		select.so

build:

clean:
	rm -f *.a *.o *.so

install:
	install -d $(DESTDIR)$(LUADIR)/linux/sys
	install select.so $(DESTDIR)$(LUADIR)/linux/sys/select.so

select.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o select.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
