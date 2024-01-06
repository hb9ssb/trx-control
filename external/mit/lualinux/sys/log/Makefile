SRCS=		lualog.c

VPATH=		../../../../mit/lua/src

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH} -D_GNU_SOURCE

all:		log.so

build:

clean:
	rm -f *.a *.o *.so

install:
	install -d $(DESTDIR)$(LUADIR)/linux/sys
	install log.so $(DESTDIR)$(LUADIR)/linux/sys/log.so

log.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o log.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
