VPATH=		../../mit/lua/src

SRCS=		luazmq.c

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH}
LDADD+=		-lpthread -lzmq

all:		zmq.so

build:

clean:
	rm -f *.o *.a *.so

install:
	install -d $(DESTDIR)$(LUADIR)
	install zmq.so $(DESTDIR)$(LUADIR)/zmq.so

zmq.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o zmq.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<

luazmq.o:	luazmq.c luazmq.h
