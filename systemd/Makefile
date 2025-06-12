UNITS=	trxd.service trxd-remote.service

all:

clean:

install:
	for f in ${UNITS}; do \
		install -D -m 644 $$f ${DESTDIR}/etc/systemd/system/$$f; \
	done
	install -D -m 644 trxd-remote.conf \
		${DESTDIR}/usr/share/trxd/trxd-remote.conf
