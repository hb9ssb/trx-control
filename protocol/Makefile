PROTO=		cat-delimited.lua \
		cat-5-byte.lua \
		ci-v.lua \
		kenwood-th-d-series.lua \
		kenwood-ts480.lua \
		rtxlink.lua \
		simulated.lua

PROTODIR?=	/usr/share/trxd/protocol

build:

clean:

install:
	@install -d ${DESTDIR}${PROTODIR}
	@for f in ${PROTO}; \
		do install -m 644 $$f ${DESTDIR}${PROTODIR}/$$f; \
	done
