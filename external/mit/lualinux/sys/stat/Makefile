SRCS=		luastat.c

VPATH=		../../../../mit/lua/src

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH} -D_GNU_SOURCE

all:		stat.so

build:

clean:
	rm -f *.a *.o *.so

install:
	install -d $(DESTDIR)$(LUADIR)/linux/sys
	install stat.so $(DESTDIR)$(LUADIR)/linux/sys/stat.so

stat.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o stat.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
