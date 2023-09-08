RIGS=		yaesu-ft-710.lua \
		yaesu-ft-897.lua \
		yaesu-ft-817.lua

RIGSDIR?=	/usr/share/rigd/rigs

build:

clean:

install:
	@install -d ${DESTDIR}${RIGSDIR}
	@for f in ${RIGS}; do install -m 644 $$f ${DESTDIR}${RIGSDIR}/$$f; done
