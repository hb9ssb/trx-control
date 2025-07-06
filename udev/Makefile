RULES=		70-bmcm-usb-pio.rules \
		70-ft-710.rules \
		70-ic-705.rules \
		70-th-d75.rules \
		70-ts-590.rules \
		70-yaesu-cat.rules

RULESDIR?=	/usr/lib/udev/rules.d

build:

clean:

install:
	@install -d ${DESTDIR}${RULESDIR}
	@for f in ${RULES}; \
		do install -m 644 $$f ${DESTDIR}${RULESDIR}/$$f; \
	done
