EXTENSION?=	config.lua dxcluster.lua hello.lua logbook.lua ping.lua \
		qrz.lua tasmota.lua memory.lua memory-db.lua hamqth.lua \
		wavelog.lua

EXTDIR?=	/usr/share/trxd/extension

build:

clean:

install:
	@install -d ${DESTDIR}${EXTDIR}
	@for f in ${EXTENSION}; \
		do install -m 644 $$f ${DESTDIR}${EXTDIR}/$$f; \
	done
