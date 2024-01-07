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

-- The dxcluster extension for trx-control

local socket = require 'linux.sys.socket'

local config = ...
local loggedIn = false;

if trxd.verbose() > 0 then
	print 'initializing the trx-control dxcluster extension'
end

local conn = socket.connect(config.host, config.port)

-- Spawn a data ready handler thread for the socket, it will call the
-- dataReady function whenever data arrives on the socket

trxd.signalInput(conn:socket(), 'dataReady')

local login= 'login:'
local deline = 'DX de ([%w/]+):%s+(%d+%p%d+)%s+([%w/]+) +([%w%s%p-]+)%s+(%d%d)(%d%d)Z'

function dataReady()
	if not loggedIn then
		local prompt = conn:read(7, 1000)
		if string.sub(prompt, 1, 6) == login then
			conn:write(config.callsign .. '\n')
			loggedIn = true
		end
	else
		local data = conn:readln()
		for spotter, frequency, spotted, message, hour, minute
		    in string.gmatch(data, deline) do
			local notification = {
				dxcluster = {
					spotter = spotter,
					frequency = (tonumber(frequency) or 0)
					    * 1000,
					spotted = spotted,
					message = message,
					time = string.format('%s:%s UTC',
					    hour, minute)
				}
			}

			trxd.notify(json.encode(notification))
		end

	end
end
