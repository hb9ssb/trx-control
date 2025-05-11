VERSION=	1.2.4
RELEASE=	3

build:

# Build instructions
SUBDIR+=	bin/bluecat \
		bin/trxctl \
		gpio \
		lib/liblua \
		lib/libtrx-control \
		extension \
		protocol \
		trx \
		sbin/trxd \
		udev \
		external/bsd/luacurl \
		external/mit/luaexpat \
		external/mit/lualinux \
		external/mit/luanode \
		external/bsd/luapgsql \
		external/bsd/luasqlite \
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

# Definitions and targets for package building
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
REPOBASE=	/var/www/trx-control/pub/repos
PKGDEST=	/tmp/trx-control

YUMDEST=	$(PKGDEST)/yum
APTDEST=	$(PKGDEST)/apt
ZYPPDEST=	$(PKGDEST)/zypp

REPOUSER=	root
REPOPATH=	$(REPOBASE)/rhel-$(RELEASEVER)-$(ARCH)

REDHAT_BASED=	alma-10-kitten \
		alma-9 \
		alma-8 \
		rocky-9 \
		rocky-8 \
		fedora-rawhide \
		fedora-41 \
		fedora-40 \
		fedora-39

SUSE_BASED=	opensuse-leap-15.6 \
		opensuse-leap-15.5 \
		opensuse-tumbleweed

DEBIAN_BASED=	ubuntu-25.04 \
		ubuntu-24.10 \
		ubuntu-24.04 \
		ubuntu-22.04 \
		ubuntu-20.04 \
		debian-13 \
		debian-12 \
		debian-11

# The following targets are used within the docker container
CODENAME?=	$(shell lsb_release -sc)
DEBBASE=	/apt/dists/$(CODENAME)

ifeq ($(ARCH), x86_64)
DEBPATH=	/apt/dists/$(CODENAME)/stable/binary-amd64
DEBARCH=	amd64
endif
ifeq ($(ARCH), aarch64)
DEBPATH=	/apt/dists/$(CODENAME)/stable/binary-arm64
DEBARCH=	arm64
endif

prepare:
	gpg --import /keys/private.asc
	gpg --import /keys/public.asc
	mkdir /build
	cp -a /dist/* /build/

redhat-packages: prepare
	rm -f ~/rpmbuild/RPMS/*/trx-control-*
	(cd /build && \
		VERSION=$(VERSION) RELEASE=$(RELEASE) PG_VERSION=$(PG_VERSION) \
		DISTRIBUTION=$@ make -C package/redhat)
	cp -a ~/rpmbuild/RPMS/* /yum

suse-packages: prepare
	rm -f ~/rpmbuild/RPMS/*/trx-control-*
	(cd /build && \
		VERSION=$(VERSION) RELEASE=$(RELEASE) PG_VERSION=$(PG_VERSION) \
		PG_CONFIG=/usr/bin/pgconfig \
		make -C package/suse)
	cp -a ~/rpmbuild/RPMS/* /zypp

debian-packages: prepare
	chmod 700 /root/.gnupg
	mkdir -p $(DEBPATH)
	(cd /build && PG_CONFIG=/usr/bin/pg_config dpkg-buildpackage -b)
	cp /trx-control*.deb $(DEBPATH)
	cd /apt && \
		dpkg-scanpackages \
		dists/$(CODENAME)/stable/binary-$(DEBARCH) > $(DEBPATH)/Packages
	gzip -c $(DEBPATH)/Packages > $(DEBPATH)/Packages.gz
	echo Origin: trx-control > $(DEBBASE)/Release
	echo Label: trx-control >> $(DEBBASE)/Release
	echo Suite: stable >> $(DEBBASE)/Release
	echo Version: $(VERSION)-$(RELEASE) >> $(DEBBASE)/Release
	echo Codename: $(CODENAME) >> $(DEBBASE)/Release
	apt-ftparchive release $(DEBBASE) >> $(DEBBASE)/Release
	gpg -a --yes --output $(DEBBASE)/Release.gpg \
		--local-user "info@hb9ssb.ch" \
		--detach-sign $(DEBBASE)/Release

packages:	$(REDHAT_BASED) $(SUSE_BASED) $(DEBIAN_BASED)
	@echo All packages built

redhat:	$(REDHAT_BASED)

suse:	$(SUSE_BASED)

debian:	$(DEBIAN_BASED)

packages-clean:
	rm -rf $(PKGDEST)

# Package building targets run on the container host

# Special targets to build rpms and install them.  Should eventually be
# be replaced by a CI/CD pipeline.

UID=	$(shell id -u)
SOCK=	/tmp/.trx-control.$(UID)

repos:	connect fetch-debian packages redhat-repo suse-repo debian-repo \
	pubkey fix-permissions remove-old-packages disconnect

repos-nb:	connect redhat-repo suse-repo debian-repo pubkey disconnect

connect:
	ssh -S $(SOCK) -M -N -o ControlMaster=yes -f root@trx-control.msys.ch
	@echo connected

fetch-debian:
	rsync -avz -e "ssh -o ControlPath=$(SOCK)" \
		root@trx-control.msys.ch:$(REPOBASE)/apt $(PKGDEST)

pubkey:
	gpg --export -a info@hb9ssb.ch | \
		ssh -S $(SOCK) trx-control.msys.ch cat \> \
		$(REPOBASE)/trx-control.asc

fix-permissions:
	ssh -S $(SOCK) trx-control.msys.ch chmod -R g+r,a+r $(REPOBASE)

remove-old-packages:
	ssh -S $(SOCK) trx-control.msys.ch find $(REPOBASE) -type f -mtime +7 \
		-exec rm {} " \;"

disconnect:
	ssh -S $(SOCK) -O exit trx-control.msys.ch
	@echo disconnected

redhat-repo:
	ssh -S $(SOCK) trx-control.msys.ch mkdir -p $(REPOBASE)/yum
	rsync -avz -e "ssh -o ControlPath=$(SOCK)" \
		$(YUMDEST) root@trx-control.msys.ch:$(REPOBASE)
	for f in $(REDHAT_BASED); \
		do \
			ssh -S $(SOCK) trx-control.msys.ch \
				createrepo $(REPOBASE)/yum/$$f; \
			ssh -S $(SOCK) trx-control.msys.ch \
				ln -f -s \
				$(REPOBASE)/yum/$$f/noarch/trx-control-repo-$(VERSION)-$(RELEASE).noarch.rpm \
				$(REPOBASE)/yum/$$f/noarch/trx-control-repo-latest.noarch.rpm; \
		done

suse-repo:
	ssh -S $(SOCK) trx-control.msys.ch mkdir -p $(REPOBASE)/zypp
	rsync -avz -e "ssh -o ControlPath=$(SOCK)" \
		$(ZYPPDEST) root@trx-control.msys.ch:$(REPOBASE)
	for f in $(SUSE_BASED); \
		do \
			ssh -S $(SOCK) trx-control.msys.ch \
				createrepo $(REPOBASE)/zypp/$$f; \
			ssh -S $(SOCK) trx-control.msys.ch \
				ln -f -s \
				$(REPOBASE)/zypp/$$f/noarch/trx-control-repo-$(VERSION)-$(RELEASE).noarch.rpm \
				$(REPOBASE)/zypp/$$f/noarch/trx-control-repo-latest.noarch.rpm; \
		done

debian-repo:
	ssh -S $(SOCK) trx-control.msys.ch mkdir -p $(REPOBASE)/apt
	rsync -avz -e "ssh -o ControlPath=$(SOCK)" \
		$(APTDEST) root@trx-control.msys.ch:$(REPOBASE)

keys:
	-mkdir -p $(PKGDEST)
	rm -f $(PKGDEST)/*.asc
	-gpg -q --output $(PKGDEST)/public.asc --armor \
		--export info@hb9ssb.ch
	-gpg -q --output $(PKGDEST)/private.asc --armor \
		--export-secret-key info@hb9ssb.ch

$(REDHAT_BASED): keys
	-mkdir -p $(YUMDEST)/$@
	docker run --rm \
		-e REPO=$@ \
		-v `pwd`:/dist \
		-v $(YUMDEST)/$@:/yum \
		-v $(PKGDEST):/keys \
		--name $@ \
		pkg-builder/$@ \
		/usr/bin/make -C /dist redhat-packages

$(SUSE_BASED): keys
	-mkdir -p $(ZYPPDEST)/$@
	docker run --rm \
		-e REPO=$@ \
		-v `pwd`:/dist \
		-v $(ZYPPDEST)/$@:/zypp \
		-v $(PKGDEST):/keys \
		--name $@ \
		pkg-builder/$@ \
		/usr/bin/make -C /dist suse-packages

$(DEBIAN_BASED): keys
	-mkdir -p $(APTDEST)
	docker run --rm \
		-e REPO=$@ \
		-v `pwd`:/dist \
		-v $(APTDEST):/apt \
		-v $(PKGDEST):/keys \
		--name $@ \
		pkg-builder/$@ \
		/usr/bin/make -C /dist debian-packages
