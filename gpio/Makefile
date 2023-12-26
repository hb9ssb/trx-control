GPIO=		bmcm-usb-pio.lua

GPIODIR?=	/usr/share/trxd/gpio

build:

clean:

install:
	@install -d ${DESTDIR}${GPIODIR}
	@for f in ${GPIO}; do install -m 644 $$f ${DESTDIR}${GPIODIR}/$$f; done
