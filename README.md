# trx-control (pronounced "transceiver control")

Software to control amateur radio transceivers.  This is work in progress.

trx-control consists of trxd(8), a daemon to control the transceivers,
and, trxctl(1), a command line utility to access trxd.  trxctl(1) is meant
as a reference client only.  Software that wants to make use of trxd(8)
should implement the protocol and talk to trxd(8) directly over the network.

trxd(8) listens on port 14285 by for incoming connections
(14285 is the default port, it can be changed). It supports both IPv4 and IPv6.

The effective transceiver control is done using Lua modules,
this way new transceivers can easily be supported by suppliying
a corresponding Lua module for a specific transceiver model.
See https://lua.org and https://lua.msys.ch for more information
on Lua.

## Supported transceivers

Initially, trx-control will support the following transceivers:

* Yaesu FT-710
* Yaesu FT-897
* Yaesu FT-817
* Yaesu FT_991A

The client/server protocol is documented in the projects wiki at
https://github.com/hb9ssb/trx-control/wiki
