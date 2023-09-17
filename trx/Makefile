TRX=		dummy-trx.lua \
		yaesu-ft-710.lua \
		yaesu-ft-897.lua \
		yaesu-ft-817.lua \
		yaesu-ft-991a.lua

TRXDIR?=	/usr/share/trxd/trx

build:

clean:

install:
	@install -d ${DESTDIR}${TRXDIR}
	@for f in ${TRX}; do install -m 644 $$f ${DESTDIR}${TRXDIR}/$$f; done
