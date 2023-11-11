SUBDIR+=	bin/trxctl \
		bin/xqrg \
		lib/liblua \
		lib/libtrx-control \
		protocol \
		trx \
		sbin/trxd

MANDIR?=	/usr/share/man

.PHONY:	build
build:		$(SUBDIR)
build:		subdir

.PHONY: subdir $(SUBDIR)
subdir: $(SUBDIR)

$(SUBDIR):
	$(MAKE) -C $@ $(TARGET)

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
