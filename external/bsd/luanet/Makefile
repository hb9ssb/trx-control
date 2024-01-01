VPATH=		../../mit/lua/src

SRCS=		luanet.c

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH} -D_GNU_SOURCE

all:		net.so

build:

clean:
	rm -f *.o *.so

install:
	install -d $(DESTDIR)$(LUADIR)
	install net.so $(DESTDIR)$(LUADIR)/net.so

net.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o net.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
