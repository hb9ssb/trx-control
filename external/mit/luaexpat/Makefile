VPATH=		../../mit/lua/src

SRCS=		buffer.c \
		luaexpat.c

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH} -D_GNU_SOURCE
LDADD+=		-lexpat

all:		expat.so

build:

clean:
	rm -f *.o *.a *.so

install:
	install -d $(DESTDIR)$(LUADIR)
	install expat.so $(DESTDIR)$(LUADIR)/expat.so

expat.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o expat.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
