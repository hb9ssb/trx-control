TRX=		icom-ic-705.yaml \
		openrtx.yaml \
		simulator.yaml \
		yaesu-ft-710.yaml \
		yaesu-ft-817.yaml \
		yaesu-ft-897.yaml

TRXDIR?=	/usr/share/trxd/trx

build:

clean:

install:
	@install -d ${DESTDIR}${TRXDIR}
	@for f in ${TRX}; do install -m 644 $$f ${DESTDIR}${TRXDIR}/$$f; done
