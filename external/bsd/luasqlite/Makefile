VPATH=		../../mit/lua/src

SRCS=		luasqlite.c

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH} -D_GNU_SOURCE
LDADD+=		-lsqlite3

all:		sqlite.so

build:

clean:
	rm -f *.o *.so

install:
	install -d $(DESTDIR)$(LUADIR)
	install sqlite.so $(DESTDIR)$(LUADIR)/sqlite.so

sqlite.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o sqlite.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
