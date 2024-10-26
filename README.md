# trx-control

trx-control is an extensible software system to control amateur radio
transceivers and related hardware like GPIO-pins etc. and to integrate
third-party software using application specific extensions.

New users should read the **trx-control User Guide** found at the URL
https://trx-control.msys.ch/#_the_trx_control_user_guide where it is
available in HTML, PDF, and, EPUB format.

Further information, project goals, a general overview, technical information,
application notes, a FAQ etc. can be found on the trx-control website at
https://trx-control.msys.ch.

## What is included?

trx-control mainly consists of trxd(8), a daemon to control the
transceivers and other hardware, and, trxctl(1), a command line utility to
access trxd(8), and, bluecat(1), a tool to help with the configuration of
bluetooth connections.

trx-control comes with protocol drivers for common transceiver
brands, configurations for a variety of transceiver models, a bunch
of extensions, and, complete documentation online as well as in the form on
manual pages.

## Using trx-control from client software

Software that wants to make use of trxd(8) should implement the trx-control
client/server protocol and talk to trxd(8) over the network, either over
raw IP sockets or WebSockets.

_(trxd(8) listens on port 14285 by default for incoming connections over
raw sockets and can optionally listen for WebSocket connections. It supports
both IPv4 and IPv6.)_

See the **trx-control Integration Guide** at
https://trx-control.msys.ch/#_the_trx_control_integration_guide for all
information on the client/server protocol, the extensions etc.

## Adding new transceivers

Transceivers are controlled using protocol drivers written in
the Lua programming language.  Properties of a specific transceiver are
defined in YAML format.  New transceivers can be added by adding a
protocol driver (if needed) and a transceiver defintion.

See the **trx-control Developer Guide** at
https://trx-control.msys.ch/#_the_trx_control_developer_guide for all
information needed to write protocol drivers and the documentation of all
Lua modules included.

See https://lua.org for more information on Lua.

## trx-control on Matrix

There is a Matrix room ``#trx-control:matrix.org`` to discuss trx-control:
https://matrix.to/#/#trx-control:matrix.org.
