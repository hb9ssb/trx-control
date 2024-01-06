SRCS=		luadirent.c

VPATH=		../../../mit/lua/src

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH} -D_GNU_SOURCE
#LDADD+=		-lbsd -lcrypt

all:		dirent.so

build:

clean:
	rm -f *.a *.o *.so

install:
	install -d $(DESTDIR)$(LUADIR)/linux
	install dirent.so $(DESTDIR)$(LUADIR)/linux/dirent.so

dirent.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o dirent.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
