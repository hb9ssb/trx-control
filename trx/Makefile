TRX=		icom-ic-705.lua \
		simulator.lua \
		yaesu-ft-710.lua \
		yaesu-ft-817.lua \
		yaesu-ft-897.lua

TRXDIR?=	/usr/share/trxd/trx

build:

clean:

install:
	@install -d ${DESTDIR}${TRXDIR}
	@for f in ${TRX}; do install -m 644 $$f ${DESTDIR}${TRXDIR}/$$f; done
