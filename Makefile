-include /etc/os-release

VERSION=	1.0.0
RELEASE=	1

PG_VERSION?=	16

# Variables needed for distribution building
ifeq ($(ID), none)
ID=	$(shell lsb_release -si)
endif

ifeq ($(ID), "almalinux")
ID="rhel"
endif

RELEASEVER=	$(shell echo $(VERSION_ID) | cut -d . -f 1)
ARCH=		$(shell uname -m)

REPOHOST=	trx-control.msys.ch
REPORPM=	trx-control-repo-latest.noarch.rpm
REPOBASE=	/var/www/trx-control/yum

REPOUSER=	root
REPOPATH=	$(REPOBASE)/rhel-$(RELEASEVER)-$(ARCH)

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
		yum

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

# Special targets to build rpms and install them.  Should eventually be
# be replaced by a CI/CD pipeline.

rpm:
	rm -f ~/rpmbuild/RPMS/*/trx-control-*
	VERSION=$(VERSION) RELEASE=$(RELEASE) PG_VERSION=$(PG_VERSION) \
		make -C package/redhat

dist:	rpm
	ssh $(REPOUSER)@$(REPOHOST) mkdir -p $(REPOPATH)
	scp ~/rpmbuild/RPMS/$(ARCH)/trx-control-* \
		~/rpmbuild/RPMS/noarch/trx-control-* \
		$(REPOUSER)@$(REPOHOST):$(REPOPATH)

	scp ~/rpmbuild/RPMS/noarch/trx-control-repo-*.rpm \
		$(REPOUSER)@$(REPOHOST):$(REPOPATH)/$(REPORPM)

	ssh $(REPOUSER)@$(REPOHOST) createrepo $(REPOPATH)

dockerized:
	mkdir /build
	cp -a /dist/* /build/
	(cd /build && make rpm)
	cp -a ~/rpmbuild/RPMS /rpms

opensuse-leap-15.5:
	-mkdir rpms/$@
	docker run --rm \
		-v `pwd`:/dist \
		-v `pwd`/../rpms/$@:/rpms \
		--name $@ \
		pkg-builder/$@ \
		/usr/bin/make -C /dist dockerized

fedora-39:
	-mkdir rpms/$@
	docker run --rm \
		-v `pwd`:/dist \
		-v `pwd`/../rpms/$@:/rpms \
		--name $@ \
		pkg-builder/$@ \
		/usr/bin/make -C /dist dockerized

fedora-38:
	-mkdir rpms/$@
	docker run --rm \
		-v `pwd`:/dist \
		-v `pwd`/../rpms/$@:/rpms \
		--name $@ \
		pkg-builder/$@ \
		/usr/bin/make -C /dist dockerized

alma-9:
	-mkdir rpms/$@
	docker run --rm \
		-v `pwd`:/dist \
		-v `pwd`/../rpms/$@:/rpms \
		--name $@ \
		pkg-builder/$@ \
		/usr/bin/make -C /dist dockerized

alma-8:
	-mkdir rpms/$@
	docker run --rm \
		-v `pwd`:/dist \
		-v `pwd`/../rpms/$@:/rpms \
		--name $@ \
		pkg-builder/$@ \
		/usr/bin/make -C /dist dockerized

rocky-9:
	-mkdir rpms/$@
	docker run --rm \
		-v `pwd`:/dist \
		-v `pwd`/../rpms/$@:/rpms \
		--name $@ \
		pkg-builder/$@ \
		/usr/bin/make -C /dist dockerized

rocky-8:
	-mkdir rpms/$@
	docker run --rm \
		-v `pwd`:/dist \
		-v `pwd`/../rpms/$@:/rpms \
		--name $@ \
		pkg-builder/$@ \
		/usr/bin/make -C /dist dockerized
