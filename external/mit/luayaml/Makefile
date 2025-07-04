VPATH=		../../mit/lua/src

SRCS=		luayaml.c anchor.c

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH}
LDADD+=		-lyaml

all:		yaml.so

build:

clean:
	rm -f *.o *.a *.so

install:
	install -d $(DESTDIR)$(LUADIR)
	install yaml.so $(DESTDIR)$(LUADIR)/yaml.so

yaml.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o yaml.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
