SRCS=		luasocket.c

VPATH=		../../../../mit/lua/src

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH} -D_GNU_SOURCE

all:		socket.so

build:

clean:
	rm -f *.a *.o *.so

install:
	install -d $(DESTDIR)$(LUADIR)/linux/sys
	install socket.so $(DESTDIR)$(LUADIR)/linux/sys/socket.so

socket.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o socket.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
