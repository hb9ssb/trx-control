TRX=		csi-cs7000-m17.yaml \
		icom-ic-705.yaml \
		kenwood-th-d75a.yaml \
		kenwood-th-d75e.yaml \
		openrtx.yaml \
		simulator.yaml \
		yaesu-ft-710.yaml \
		yaesu-ft-817.yaml \
		yaesu-ft-857.yaml \
		yaesu-ft-891.yaml \
		yaesu-ft-897.yaml \
		yaesu-ft-991a.yaml

TRXDIR?=	/usr/share/trxd/trx

build:

clean:

install:
	@install -d ${DESTDIR}${TRXDIR}
	@for f in ${TRX}; do install -m 644 $$f ${DESTDIR}${TRXDIR}/$$f; done
