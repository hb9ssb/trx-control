VERSION=	1.0.0
RELEASE=	1

SUBDIR+=	bin/trxctl \
		bin/xqrg \
		gpio \
		lib/liblua \
		lib/libtrx-control \
		extension \
		protocol \
		trx \
		sbin/trxd \
		udev \
		external/bsd/luacurl \
		external/bsd/luanet \
		external/bsd/luapgsql \
		external/bsd/luasqlite \
		external/mit/luaexpat

MANDIR?=	/usr/share/man

.PHONY:	build
build:		$(SUBDIR)
build:		subdir

.PHONY: subdir $(SUBDIR)
subdir: $(SUBDIR)

$(SUBDIR):
	VERSION=$(VERSION) $(MAKE) -C $@ $(TARGET)

# Recursive targets
.PHONY: clean
clean:		TARGET=clean
clean: 		subdir

.PHONY: cleandir
cleandir:	TARGET=cleandir
cleandir: 	subdir

.PHONY: install trx-control.7
install:	TARGET=install
install: 	subdir trx-control.7
	@echo all installed

trx-control.7:
		@install -D -m 644 $@ $(DESTDIR)$(MANDIR)/man7/$@
		@gzip -f $(DESTDIR)$(MANDIR)/man7/$@

# Dependencies

bin/trxctl:	lib/libtrx-control lib/liblua
bin/xqrg:	lib/libtrx-control lib/liblua
sbin/trxd:	lib/libtrx-control lib/liblua

rpm:
	VERSION=$(VERSION) RELEASE=$(RELEASE) make -C package/redhat
