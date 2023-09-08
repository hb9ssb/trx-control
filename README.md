# rig-control

Software to control amateur radio transceivers.

rig-control consists of rigd(8), a daemon to control the transceiver,
and, rigctl(1), a command line utility to access rigd.

rigd(8) listens on port 14285 by for incoming connections
(14285 is the default port, it can be changed).

The effective transceiver control is done using Lua modules,
this way new transceivers can easily be supported by suppliying
a corresponding Lua module for a specific transceiver model.
See https://lua.org and https://lua.msys.ch for more information
on Lua.

# Background

rig-control is the base layer of a larger software system to
support contesting in "runner" and "search & pounce" mode where
a monitor station can enter heard stations into a queue (querying
rigd(8) for frequency an mode) and the "run & pounce" station
can either select a running frequency and mode or select a station
from the "stations heard" queue. By clicking or tapping on a
running frequency or en antry from the "stations heard" queue,
the transceiver will be tuned to that frequency using rigd(8).

Another client might be the contest logging software, which can
query rigd(8) for the ferquency and mode of the station that
is currently being worked.  We have plans to add rig-control
support to SkookumLogger.

# Supported transceivers

Initially, rig-control will support the following transceivers:

* Yaesu FT-710
* Yaesu FT-897
* Yaesu FT-817

# Protocol

The protocol between rigd(8) and a client will be simple, but
is not yet defined in its entity. In any case, one rigd(8)
daemon can serve an unlimited number of clients.  A client can
not only send commands to rigd(8), it can also subscribe for
events like frequency changed etc.
