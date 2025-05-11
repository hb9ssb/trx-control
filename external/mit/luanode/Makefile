VPATH=		../../mit/lua/src

SRCS=		luanode.c

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH}
LDADD+=		-lyaml

all:		node.so

build:

clean:
	rm -f *.o *.a *.so

install:
	install -d $(DESTDIR)$(LUADIR)
	install node.so $(DESTDIR)$(LUADIR)/node.so

node.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o node.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
