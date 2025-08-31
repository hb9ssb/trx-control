SINKS=	10-ft-710-sink.conf \
	10-ic-705-sink.conf

all:

clean:

install:
	for f in ${SINKS}; do \
		install -D -m 644 $$f \
			${DESTDIR}/usr/share/pipewire/pipewire.conf.d/$$f; \
	done
