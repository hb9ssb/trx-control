# This Makefile is used on the target OS to build trx-control locally

-include /etc/os-release
include Makefile.version

build:

# Build instructions
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
		external/bsd/luapgsql \
		external/bsd/luasqlite \
		external/mit/luaexpat \
		external/mit/lualinux \
		external/mit/luayaml \
		yum \
		zypp \
		systemd

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

external/mit/luayaml:	sbin/trxd

# Package building
packages:
	make -f Makefile.packages -j 4

packages-clean:
	make -f Makefile.packages clean

# Build packages and send them to the repository
repos:	packages
	make -f Makefile.packages repos

# Same, without building packages
repos-nb:
	make -f Makefile.packages repos
