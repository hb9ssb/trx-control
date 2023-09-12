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

trxd(8) waits for incoming connections and accepts a series of commands.

### set-frequency <frequency>

Set the current frequency to <frequency>.

### get-frequency

Return the current frequency.

### listen-frequency

Listen to frequency changes.  If the frequency changes, the client will
receive a string in the form "frequency 14285".

At the moment, this works only if the frequency is changed by a trxd (8)
client, but not if the frequency is changed directly on the receiver. This
will change in the near future.

### unlisten-frequency

No longer listen for frequency changes.

### get-tranceiver

Return the curren transceiver type.

### load-driver <trx-type>

Load driver for <trx-type> at runtime, replacing the currently active driver.
