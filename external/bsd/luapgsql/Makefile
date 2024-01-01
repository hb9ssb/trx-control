VPATH=		../../mit/lua/src

SRCS=		luapgsql.c
PG_CONFIG?=	/usr/pgsql-16/bin/pg_config

LUADIR?=	/usr/share/trxd/lua

OBJS=		${SRCS:.c=.o}

CFLAGS+=	-I${VPATH} -I$(shell $(PG_CONFIG) --includedir)
LDADD+=		-L$(shell $(PG_CONFIG) --libdir) -lpq

all:		pgsql.so

build:

clean:
	rm -f *.o *.so

install:
	install -d $(DESTDIR)$(LUADIR)
	install pgsql.so $(DESTDIR)$(LUADIR)/pgsql.so

pgsql.so:	${OBJS}
		$(CC) -shared -fPIC -O3 -o pgsql.so ${CFLAGS} ${OBJS} ${LDADD}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
