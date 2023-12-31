VPATH=		../../mit/lua/src

SRCS=		constants.c \
		luacurl.c \
		multi.c

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH}
LDADD+=		-lcurl

all:		curl.so

build:

clean:
	rm -f *.o *.a *.so

install:
	install -d $(DESTDIR)$(LUADIR)
	install curl.so $(DESTDIR)$(LUADIR)/curl.so

curl.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o curl.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
