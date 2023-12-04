SRCS=		trx-control.c

OBJS=		${SRCS:.c=.o}

all:		libtrx-control.a

build:

clean:
	rm -f *.o *.a

install:

libtrx-control.a:	${OBJS}
		ar rcs libtrx-control.a ${OBJS}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<

trx-control.o:	trx-control.c trx-control.h
