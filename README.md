# trx-control (pronounced "transceiver control")

Software to control amateur radio transceivers.  This is work in progress.

trx-control consists of trxd(8), a daemon to control the transceiver,
and, trxctl(1), a command line utility to access trxd.

trxd(8) listens on port 14285 by for incoming connections
(14285 is the default port, it can be changed).

trx-control supports IPv4 and IPv6.

The effective transceiver control is done using Lua modules,
this way new transceivers can easily be supported by suppliying
a corresponding Lua module for a specific transceiver model.
See https://lua.org and https://lua.msys.ch for more information
on Lua.

## Background

trx-control (pronounced as "transceiver control") is the base layer of a
larger software system to support contesting in "runner" and "search & pounce"
mode where a monitor station can enter heard stations into a queue (querying
trxd(8) for frequency an mode) and the "run & pounce" station
can either select a running frequency and mode or select a station
from the "stations heard" queue. By clicking or tapping on a
running frequency or en antry from the "stations heard" queue,
the transceiver will be tuned to that frequency using trxd(8).

Another client might be the contest logging software, which can
query trxd(8) for the frequency and mode of the station that
is currently being worked.

## Supported transceivers

Initially, trx-control will support the following transceivers:

* Dummy transceiver for testing purposes
* Yaesu FT-710
* Yaesu FT-897
* Yaesu FT-817

## Protocol

The protocol between trxd(8) and a client will be simple, but
is not yet defined in its entity. In any case, one trxd(8)
daemon can serve an unlimited number of clients.  A client can
not only send commands to trxd(8), it can also subscribe for
events like frequency changed etc.
