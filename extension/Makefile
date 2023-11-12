EXTENSION=	ping.lua

EXTDIR?=	/usr/share/trxd/extension

build:

clean:

install:
	@install -d ${DESTDIR}${EXTDIR}
	@for f in ${EXTENSION}; \
		do install -m 644 $$f ${DESTDIR}${EXTDIR}/$$f; \
	done
