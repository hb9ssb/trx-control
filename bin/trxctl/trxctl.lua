-- Copyright (c) 2023 Marc Balmer HB9SSB
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

-- trxctl support functions/command handlers

local function params(data)
end

local function useTrx(trx)
	local d = {
		request = 'select-trx',
		name = trx
	}

	trxctl.writeln(json.encode(d))
	local reply = json.decode(trxctl.readln())
	if reply.status == 'Ok' then
		print(string.format('now using the %s transceiver', reply.name))
	else
		print(string.format('%s: %s', reply.status, reply.reason))
	end
end

local function listTrx()
	local request = {
		request = 'list-trx'
	}

	trxctl.writeln(json.encode(request))
	local reply = json.decode(trxctl.readln())

	if reply.status == 'Ok' then
		for k, v in pairs(reply.data) do
			print(string.format('%-20s %s on %s', v.name, v.driver,
			    v.device))
		end
	else
		print(string.format('%s: %s', reply.status, reply.reason))
	end
end

local function lockTrx()
	local d = {
		request = 'lock-trx'
	}

	trxctl.writeln(json.encode(d))
	local reply = json.decode(trxctl.readln())
	if reply.status == 'Ok' then
		print('transceiver locked')
	else
		print(string.format('%s: %s', reply.status, reply.reason))
	end
end

local function unlockTrx()
	local d = {
		request = 'unlock-trx'
	}

	trxctl.writeln(json.encode(d))
	local reply = json.decode(trxctl.readln())
	if reply.status == 'Ok' then
		print('transceiver unlocked')
	else
		print(string.format('%s: %s', reply.status, reply.reason))
	end
end
local function setFrequency(freq)
	local request = {
		request = 'set-frequency',
		frequency = freq
	}

	trxctl.writeln(json.encode(request))
	local reply = json.decode(trxctl.readln())
	print(reply.frequency)
end

local function getFrequency(freq)
	local request = {
		request = 'get-frequency'
	}

	trxctl.writeln(json.encode(request))
	local reply = json.decode(trxctl.readln())
	print(reply.frequency)
end

local function setMode(mode)
	local request = {
		request = 'set-mode'
	}

	for band, mode in string.gmatch(mode, "(%w+) +(%w+)") do
		request.band = band
		request.mode = mode
	end

	trxctl.writeln(json.encode(request))
	local reply = json.decode(trxctl.readln())
	print(string.format('set mode of band %s to %s', reply.band,
	    reply.mode))
end

local function getMode()
	local request = {
		request = 'get-mode'
	}

	trxctl.writeln(json.encode(request))
	local reply = json.decode(trxctl.readln())
	print(reply.mode)
end

local function startStatusUpdates()
	local request = {
		request = 'start-status-updates'
	}
	trxctl.writeln(json.encode(request))
	local reply = json.decode(trxctl.readln())
	print(reply.mode)

end

local function stopStatusUpdates()
	local request = {
		request = 'stop-status-updates'
	}
	trxctl.writeln(json.encode(request))
	local reply = json.decode(trxctl.readln(q))
	print(reply.mode)
end

return {
	useTrx = useTrx,
	listTrx = listTrx,
	lockTrx = lockTrx,
	unlockTrx = unlockTrx,
	setFrequency = setFrequency,
	getFrequency = getFrequency,
	setMode = setMode,
	getMode = getMode,
	startStatusUpdates = startStatusUpdates,
	stopStatusUpdates = stopStatusUpdates
}
