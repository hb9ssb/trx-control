-- Copyright (c) 2024 Marc Balmer HB9SSB
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to
-- deal in the Software without restriction, including without limitation the
-- rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
-- sell copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
-- FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
-- IN THE SOFTWARE.

-- The dxcluster extension for trx-control.  This can also be used for the
-- SOTA cluster.

local socket = require 'linux.sys.socket'

local config = ...
local loggedIn = false;

if trxd.verbose() > 0 then
	print 'initializing the trx-control dxcluster extension'
end

local cacheTime = config.cacheTime or 3600

local conn = socket.connect(config.host, config.port)

-- Spawn a data ready handler thread for the socket, it will call the
-- dataReady function whenever data arrives on the socket

trxd.signalInput(conn:socket(), 'dataReady')

local login= 'login:'
local deline = 'DX de ([%w/]+):%s+(%d+%p%d+)%s+([%w/]+) +([%w%s%p-]+)%s+(%d%d)(%d%d)Z'

-- Keep a cache of spots received, invalid entries that are older than
-- cacheTime.  If cacheTime is not set, if defaults to 3600 seconds, 1 hour.

local spots = {}

local function purgeSpots()
	for k, v in pairs(spots) do
		if (v.timestamp + cacheTime) < os.time() then
			spots[k] = nil
		end
	end
end

-- dataReady is called when new data from, the cluster arrives
function dataReady()
	if not loggedIn then
		local prompt = conn:readln(1000)
		if prompt ~= nil and string.sub(prompt, 1, 6) == login then
			conn:write(config.callsign .. '\n')
			loggedIn = true
		end
	else
		local data = conn:readln()
		if data ~= nil then
			for spotter, frequency, spotted, message, hour, minute
			    in string.gmatch(data, deline) do
				local spot = {
					spotter = spotter,
					frequency = string.format('%d',
					    (tonumber(frequency) or 0)
					    * 1000),
					spotted = spotted,
					message = message,
					time = string.format('%s:%s UTC',
					    hour, minute)
				}
				spots[#spots + 1] = {
					spot = spot,
					timestamp = os.time()
				}
				local notification = {
					[config.source or 'dxcluster'] = spot
				}
				trxd.notify(json.encode(notification))
				purgeSpots()
			end
		end
	end
end

-- Return a list of spots in reverse order, i.e. newest spots first.
-- The optional parameter maxSlots can be used to limit the number of spots
-- returned.

function getSpots(request)
	local spotList = {}
	local n = 0

	local count = tonumber(request.maxSlots)
	purgeSpots()

	table.sort(spots, function (a, b) return a.timestamp > b.timestamp end)

	for k, v in pairs(spots) do
		spotList[#spotList + 1] = v.spot
		n = n + 1
		if count ~= nil and n == count then
			break
		end
	end

	return {
		status = 'Ok',
		source = config.source or 'dxcluster',
		spots = spotList
	}
end
