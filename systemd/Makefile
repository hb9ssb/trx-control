UNITS=	trx-control.service

all:

clean:

install:
	for f in ${UNITS}; do \
		install -D -m 644 $$f ${DESTDIR}/etc/systemd/system/$$f; \
	done
