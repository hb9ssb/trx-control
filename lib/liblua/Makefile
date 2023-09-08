VPATH=		../../external/mit/lua/src

LUA_CORE=	lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c \
		llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c \
		lstring.c ltable.c ltm.c lundump.c lvm.c lzio.c
LUA_LIB=        lauxlib.c lbaselib.c lcorolib.c ldblib.c liolib.c \
		lmathlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c loadlib.c \
		linit.c

SRCS=		${LUA_CORE} ${LUA_LIB}
OBJS=		${SRCS:.c=.o}

CFLAGS+=	-DLUA_USE_LINUX -I${VPATH}

all:		liblua.a

build:

clean:
	rm -f *.o *.a
	
install:

liblua.a:	${OBJS}
		ar rcs liblua.a ${OBJS}

.c.o:
		cc -O3 -fPIC -c -o $@ ${CFLAGS} $<
