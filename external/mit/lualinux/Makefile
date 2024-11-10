SRCS=		lualinux.c

VPATH=		../../mit/lua/src

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH} -D_GNU_SOURCE
LDADD+=		-lbsd -lcrypt

all:		linux.so
	make -C dirent
	make -C dl
	make -C pwd
	make -C sys

build:

clean:
	rm -f *.a *.o *.so
	make -C dirent clean
	make -C dl clean
	make -C pwd clean
	make -C sys clean

install:	linux.so
	install linux.so $(DESTDIR)$(LUADIR)/linux.so
	make -C dirent install
	make -C dl install
	make -C pwd install
	make -C sys install

linux.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o linux.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
