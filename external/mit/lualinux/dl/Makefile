SRCS=		luadl.c

VPATH=		../../../mit/lua/src

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH} -D_GNU_SOURCE
#LDADD+=		-lbsd -lcrypt

all:		dl.so

build:

clean:
	rm -f *.a *.o *.so

install:
	install -d $(DESTDIR)$(LUADIR)/linux
	install dl.so $(DESTDIR)$(LUADIR)/linux/dl.so

dl.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o dl.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
