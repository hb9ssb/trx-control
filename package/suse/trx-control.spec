Summary: trx-control
Group: hb9ssb/trx-control
Name: trx-control
Version: %{version}
Release: %{release}
Vendor: micro systems <https://msys.ch>
Packager: https://msys.ch
License: MIT
Source: trx-control-%{version}.tar.gz
Prefix: /usr
BuildRequires: curl-devel libexpat-devel gcc libyaml-devel make motif-devel
BuildRequires: openssl-devel postgresql%{pg_version}-devel readline-devel
BuildRequires: sqlite-devel

Requires: epel-release expat libcurl libyaml motif openssl postgresql16-libs
Requires: readline sqlite-libs

Provides: trx-control

Buildroot: /tmp/trx-control

%package repo
BuildArch: noarch
Summary: trx-control repository for OpenSUSE Leap and OpenSUSE Tumbleweed

%description
A modern and extensible software system to control transceivers and other
devices over the network by exchaning JSON formatted data packesg over
plain TCP/IP sockets or WebSockets.

%description repo
trx-control software repository

%files
/etc/systemd/system/trx-control.service
/etc/X11/app-defaults/XQRG
/usr/bin/trxctl
/usr/bin/xqrg
/usr/lib/udev/rules.d/70-bmcm-usb-pio.rules
/usr/lib/udev/rules.d/70-ft-710.rules
/usr/lib/udev/rules.d/70-ic-705.rules
/usr/lib/udev/rules.d/70-yaesu-cat.rules
/usr/sbin/trxd
/usr/share/man/man1/trxctl.1.gz
/usr/share/man/man1/xqrg.1.gz
/usr/share/man/man7/trx-control.7.gz
/usr/share/man/man8/trxd.8.gz
/usr/share/trxctl/trxctl.lua
/usr/share/trxd/extension/config.lua
/usr/share/trxd/extension/dxcluster.lua
/usr/share/trxd/extension/keepalive.lua
/usr/share/trxd/extension/logbook.lua
/usr/share/trxd/extension/ping.lua
/usr/share/trxd/extension/qrz.lua
/usr/share/trxd/extension/tasmota.lua
/usr/share/trxd/gpio-controller.lua
/usr/share/trxd/gpio/bmcm-usb-pio.lua
/usr/share/trxd/lua/curl.so
/usr/share/trxd/lua/expat.so
/usr/share/trxd/lua/linux.so
/usr/share/trxd/lua/linux/sys/log.so
/usr/share/trxd/lua/linux/sys/select.so
/usr/share/trxd/lua/linux/sys/socket.so
/usr/share/trxd/lua/linux/sys/stat.so
/usr/share/trxd/lua/linux/dirent.so
/usr/share/trxd/lua/linux/dl.so
/usr/share/trxd/lua/linux/pwd.so
/usr/share/trxd/lua/pgsql.so
/usr/share/trxd/lua/sqlite.so
/usr/share/trxd/lua/yaml.so
/usr/share/trxd/protocol/cat-5-byte.lua
/usr/share/trxd/protocol/cat-delimited.lua
/usr/share/trxd/protocol/ci-v.lua
/usr/share/trxd/protocol/simulated.lua
/usr/share/trxd/trx-controller.lua
/usr/share/trxd/trx/icom-ic-705.lua
/usr/share/trxd/trx/simulator.lua
/usr/share/trxd/trx/yaesu-ft-710.lua
/usr/share/trxd/trx/yaesu-ft-817.lua
/usr/share/trxd/trx/yaesu-ft-891.lua
/usr/share/trxd/trx/yaesu-ft-897.lua
/usr/share/trxd/trx/yaesu-ft-991a.lua
/usr/share/trxd/trxd.yaml
/usr/share/xqrg/xqrg.lua

%files repo
%config(noreplace) /etc/zypp/trx-control.repo
%exclude /etc/yum.repos.d/trx-control.repo

%global debug_package %{nil}
%prep
%setup -q

%build
PG_CONFIG=/usr/bin/pg_config %{__make} %{_smp_mflags}

%install
DESTDIR=$RPM_BUILD_ROOT %{__make} %{_smp_mflags} install

%clean
rm -rf $RPM_BUILD_ROOT
%{__make} clean

%changelog
* Sun Feb 11 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- trx-control 1.0.4: Adding the tasmota extension.

* Sun Feb 11 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- trx-control 1.0.3: Bug fixes.

* Sun Feb 11 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- trx-control 1.0.2: Bug fixes.

* Wed Feb 7 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- trx-control-1.0.1: Add the keepalive extension

* Mon Jan 1 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- trx-control-1.0.0: Initial version
