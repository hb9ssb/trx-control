PACKAGE=	trx-control
PKGVER?=	$(VERSION)-$(RELEASE)
DESCRIPTION=	"trx-control"
PACKAGER=	Marc Balmer HB9SSB <info@hb9ssb.radio>
URL=		https://trx-control.msys.ch
MAINTAINER=	Marc Balmer HB9SSB <info@hb9ssb.radio>
LICENSE=	MIT

FILES=		etc/systemd/system/trxd.service \
		usr/bin/bluecat \
		usr/bin/trxctl \
		usr/lib/udev/rules.d/70-bmcm-usb-pio.rules \
		usr/lib/udev/rules.d/70-ft-710.rules \
		usr/lib/udev/rules.d/70-ic-705.rules \
		usr/lib/udev/rules.d/70-th-d75.rules \
		usr/lib/udev/rules.d/70-yaesu-cat.rules \
		usr/sbin/trxd \
		usr/share/man/man1/bluecat.1.gz \
		usr/share/man/man1/trxctl.1.gz \
		usr/share/man/man7/trx-control.7.gz \
		usr/share/man/man8/trxd.8.gz \
		usr/share/trxctl/trxctl.lua \
		usr/share/trxd/extension/config.lua \
		usr/share/trxd/extension/dxcluster.lua \
		usr/share/trxd/extension/hamqth.lua \
		usr/share/trxd/extension/hello.lua \
		usr/share/trxd/extension/logbook.lua \
		usr/share/trxd/extension/memory.lua \
		usr/share/trxd/extension/memory-db.lua \
		usr/share/trxd/extension/ping.lua \
		usr/share/trxd/extension/qrz.lua \
		usr/share/trxd/extension/tasmota.lua \
		usr/share/trxd/extension/wavelog.lua \
		usr/share/trxd/gpio-controller.lua \
		usr/share/trxd/gpio/bmcm-usb-pio.lua \
		usr/share/trxd/lua/curl.so \
		usr/share/trxd/lua/expat.so \
		usr/share/trxd/lua/linux.so \
		usr/share/trxd/lua/linux/sys/log.so \
		usr/share/trxd/lua/linux/sys/select.so \
		usr/share/trxd/lua/linux/sys/socket.so \
		usr/share/trxd/lua/linux/sys/stat.so \
		usr/share/trxd/lua/linux/dirent.so \
		usr/share/trxd/lua/linux/dl.so \
		usr/share/trxd/lua/linux/pwd.so \
		usr/share/trxd/lua/node.so \
		usr/share/trxd/lua/pgsql.so \
		usr/share/trxd/lua/sqlite.so \
		usr/share/trxd/lua/yaml.so \
		usr/share/trxd/protocol/cat-5-byte.lua \
		usr/share/trxd/protocol/cat-delimited.lua \
		usr/share/trxd/protocol/ci-v.lua \
		usr/share/trxd/protocol/kenwood-th-d-series.lua \
		usr/share/trxd/protocol/kenwood-ts480.lua \
		usr/share/trxd/protocol/simulated.lua \
		usr/share/trxd/protocol/rtxlink.lua \
		usr/share/trxd/trx-controller.lua \
		usr/share/trxd/trx/csi-cs7000-m17.yaml \
		usr/share/trxd/trx/icom-ic-705.yaml \
		usr/share/trxd/trx/kenwood-th-d75a.yaml \
		usr/share/trxd/trx/kenwood-th-d75e.yaml \
		usr/share/trxd/trx/openrtx.yaml \
		usr/share/trxd/trx/simulator.yaml \
		usr/share/trxd/trx/yaesu-ft-710.yaml \
		usr/share/trxd/trx/yaesu-ft-817.yaml \
		usr/share/trxd/trx/yaesu-ft-897.yaml \
		usr/share/trxd/trx/yaesu-ft-991a.yaml \
		usr/share/trxd/trxd.yaml

BINARIES=	usr/bin/bluecat \
		usr/bin/trxctl \
		usr/sbin/trxd

DEPENDS=	readline \
		bluez \
		curl \
		expat \
		yaml

PROVIDES=	trx-control

include ../alpine.package.mk
