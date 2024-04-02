#!/usr/bin/env python

# This short code snippet only connects to trxd over a WebSocket and
# waits for data (which it never gets).  This is to pythons built-in
# WebSocket ping frame support (i.e. if they are correctly handled on
# the server side.)

import asyncio
import websockets

async def main():
	uri = "ws://localhost:14290/trx-control"

	async with websockets.connect(uri = uri,
				      ping_interval = 10,
				      ping_timeout = 60) as websocket:
		print("connected")

		await websocket.recv()

if __name__ == "__main__":
	asyncio.run(main())
