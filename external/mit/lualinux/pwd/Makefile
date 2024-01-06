SRCS=		luapwd.c

VPATH=		../../../mit/lua/src

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH} -D_GNU_SOURCE

all:		pwd.so

build:

clean:
	rm -f *.a *.o *.so

install:
	install -d $(DESTDIR)$(LUADIR)/linux
	install pwd.so $(DESTDIR)$(LUADIR)/linux/pwd.so

pwd.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o pwd.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
