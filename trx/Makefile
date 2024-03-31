TRX=		icom-ic-705.lua \
		icom-ic-7300.lua \
		simulator.lua \
		yaesu-ft-710.lua \
		yaesu-ft-817.lua \
		yaesu-ft-857.lua \
		yaesu-ft-891.lua \
		yaesu-ft-897.lua \
		yaesu-ft-991a.lua

TRXDIR?=	/usr/share/trxd/trx

build:

clean:

install:
	@install -d ${DESTDIR}${TRXDIR}
	@for f in ${TRX}; do install -m 644 $$f ${DESTDIR}${TRXDIR}/$$f; done
