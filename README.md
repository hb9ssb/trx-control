# trx-control

Extensible software to control amateur radio transceivers and related hardware
like relays, GPIO-pins and rotators and to integration clients with third-party
software using application specific extensions.

trx-control consists of trxd(8), a daemon to control the transceivers and
other hardware, trxctl(1), a command line utility to access trxd(8), and,
xqrg(1), a sample graphical client to display a transceivers frequency.
trxctl(1) and xqrg(1) are meant as reference clients only.
Software that wants to make use of trxd(8) should implement the protocol
and talk to trxd(8) directly over the network.

trxd(8) listens on port 14285 by default for incoming connections over
plain sockets and can optionally listen for WebSocket connections. It supports
both IPv4 and IPv6.

The actual transceiver control is done using Lua modules, this way new
transceivers can easily be supported by supplying a corresponding Lua driver
module for a specific transceiver model. See https://lua.org and
https://lua.msys.ch for more information on Lua.

## Supported transceivers

trx-control supports the following transceivers, more will be added over time:

* ICOM IC-705
* Yaesu FT-710
* Yaesu FT-891
* Yaesu FT-897
* Yaesu FT-817
* Yaesu FT-991a

A special simulator transceiver driver exists for development and testing
purposes.

## More information

More information can be found the wiki at
https://github.com/hb9ssb/trx-control/wiki
