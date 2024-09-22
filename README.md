# trx-control

Extensible software to control amateur radio transceivers and related hardware
like GPIO-pins etc. and to integrate clients with third-party
software using application specific extensions.

trx-control consists of trxd(8), a daemon to control the transceivers and
other hardware, and, trxctl(1), a command line utility to access trxd(8).

Software that wants to make use of trxd(8) should implement the protocol
and talk to trxd(8) directly over the network.

trxd(8) listens on port 14285 by default for incoming connections over
plain sockets and can optionally listen for WebSocket connections. It supports
both IPv4 and IPv6.

The actual transceiver control is done using protocol drivers written in
the Lua programming language.  Definitions for specific transceivers are
written in the YAML text format.

This way new transceivers can easily be supported by supplying a corresponding
protocol driver and transceiver defintion.

See https://lua.org and https://lua.msys.ch for more information on Lua.

## trx-control on Matrix

There is a Matrix room ``#trx-control:matrix.org`` to discuss trx-control:
https://matrix.to/#/#trx-control:matrix.org.

## More information

More information and the list of devices and extensions can be found on
the trx-control website https://trx-control.msys.ch.

There is also a wiki at https://github.com/hb9ssb/trx-control/wiki
