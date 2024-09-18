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
BuildRequires: libavahi-devel
BuildRequires: curl-devel libexpat-devel gcc libyaml-devel make
BuildRequires: openssl-devel postgresql%{pg_version}-devel readline-devel
BuildRequires: sqlite-devel

Requires: expat libcurl libyaml openssl postgresql16-libs
Requires: readline sqlite-libs

Provides: trx-control

Buildroot: /tmp/trx-control

%package repo
BuildArch: noarch
Summary: trx-control repository for OpenSUSE Leap and OpenSUSE Tumbleweed

%description
A modern and extensible software system to control transceivers and other
devices over the network by exchanging JSON formatted data packesg over
plain TCP/IP sockets or WebSockets.

%description repo
trx-control software repository

%files
/etc/systemd/system/trxd.service
/usr/bin/trxctl
/usr/lib/udev/rules.d/70-bmcm-usb-pio.rules
/usr/lib/udev/rules.d/70-ft-710.rules
/usr/lib/udev/rules.d/70-ic-705.rules
/usr/lib/udev/rules.d/70-th-d75.rules
/usr/lib/udev/rules.d/70-yaesu-cat.rules
/usr/sbin/trxd
/usr/share/man/man1/trxctl.1.gz
/usr/share/man/man7/trx-control.7.gz
/usr/share/man/man8/trxd.8.gz
/usr/share/trxctl/trxctl.lua
/usr/share/trxd/extension/config.lua
/usr/share/trxd/extension/dxcluster.lua
/usr/share/trxd/extension/hamqth.lua
/usr/share/trxd/extension/logbook.lua
/usr/share/trxd/extension/memory.lua
/usr/share/trxd/extension/memory-db.lua
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
/usr/share/trxd/protocol/kenwood-ts480.lua
/usr/share/trxd/protocol/simulated.lua
/usr/share/trxd/protocol/trxlink.lua
/usr/share/trxd/trx-controller.lua
/usr/share/trxd/trx/icom-ic-705.lua
/usr/share/trxd/trx/icom-ic-7300.lua
/usr/share/trxd/trx/openrtx.lua
/usr/share/trxd/trx/simulator.lua
/usr/share/trxd/trx/yaesu-ft-710.lua
/usr/share/trxd/trx/yaesu-ft-817.lua
/usr/share/trxd/trx/yaesu-ft-857.lua
/usr/share/trxd/trx/yaesu-ft-891.lua
/usr/share/trxd/trx/yaesu-ft-897.lua
/usr/share/trxd/trx/yaesu-ft-991a.lua
/usr/share/trxd/trxd.yaml

%pre
if [ "$1" == "1" ]; then
	groupadd -frg 100 trxd
	useradd -c "trx-control trxd(8) daemon" -d /home/trxd -g trxd -m trxd
fi

%post
systemctl daemon-reload
if [ "$1" == "1" ]; then
	systemctl enable trx-control
else
	systemctl -q is-enabled trx-control && systemctl restart trx-control
fi

%preun
if [ "$1" == "0" ]; then
	systemctl disable --now trx-control
fi

%postun
if [ "$1" == "0" ]; then
	userdel -r trxd
	groupdel trxd
fi

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
* Wed Sep 18 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

 - Add experimental support for OpenRTX TRXLink

* Sun Jul 21 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

 - Fix handling of ping frames without a payload in the WebSocket code.
 - The keepalive extension has been removed.

* Sun Apr 28 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

 - trxd(8) marks the default transceiver as such when sending the list of
   destinations.

* Sun Apr 7 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

 - Document long options in the manual pages.
 - trxctl(1) can directly take commands passed as arguments.
 - Use @ instead of ! to specify a destination in trxctl(1).
 - Fix a bug in the dispatcher thread.

* Sat Apr 6 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- Make NMEA fix data available via the nmea internal destination.
- Add the concept of internal destinations.

* Mon Apr 1 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- Decode NMEA sentences in the nmea-handler.
- Add the Kenwood TS-480 protocol and trusdx driver.
- Configurable transceiver and controller address of an ICOM transceiver.

* Sun Mar 31 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- Add a driver for the ICOM IC-7300 transceiver.

* Thu Mar 28 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- Add a driver for the Yaesu FT-857 transceiver.

* Thu Mar 28 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- Added the hamqth extension to lookup callsigns in the HamQTH.com database.

* Wed Mar 27 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- Memory groups and memories can be added.

* Tue Mar 26 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- Fix WebSocket ping frame handling.

* Mon Mar 25 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- JSON protocol changes.

* Wed Mar 20 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- Make the Bluetooth RFCOMM channel configurable.

* Tue Mar 19 2024 Marc Balmer HB9SSB <info@hb9ssb.ch

- Add bluetooth connectivity.

* Sat Mar 9 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- Add service autodiscovery using Avahi

* Mon Feb 19 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- Bug fixes in the WebSocket payload calculation.

* Thu Feb 15 2024 Marc Balmer HB9SSB <info@hb9ssb.ch>

- Bug fixes in the WebSocket implementation.

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
