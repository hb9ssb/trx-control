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

-- xqrg support / input handler

local trx, host, port = ...

local function useTrx(trx)
	local d = {
		request = 'select-trx',
		name = trx
	}

	xqrg.writeln(json.encode(d))
	local reply = json.decode(xqrg.readln())
	if reply.status ~= 'Ok' then
		print(string.format('%s: %s', reply.status, reply.reason))
	end
end

local function getFrequency(freq)
	local request = {
		request = 'get-frequency'
	}

	xqrg.writeln(json.encode(request))
	local reply = json.decode(xqrg.readln())
	return reply.frequency
end

local function getMode()
	local request = {
		request = 'get-mode'
	}

	xqrg.writeln(json.encode(request))
	local reply = json.decode(xqrg.readln())
	return reply.mode
end

local function startStatusUpdates()
	local request = {
		request = 'start-status-updates'
	}
	xqrg.writeln(json.encode(request))
	local reply = json.decode(xqrg.readln())
end

local function ts(s)
	s = tostring(s)
	return s:reverse():gsub('(%d%d%d)', '%1.'):reverse():gsub('^.', '')
end

useTrx(trx)

local frequency = getFrequency()
local mode = getMode()

xqrg.position(0)
xqrg.addstring(string.format('%13s Hz %s', ts(frequency), string.upper(mode)))

xqrg.status(string.format('Connected to %s@%s:%s', trx, host, port))

startStatusUpdates()

local function dataHandler(s)
	local data = json.decode(s)
	if data == nil then
		print('Undecodable string')
	elseif data.status.frequency ~= nil and data.status.mode ~= nil then
		xqrg.clrscr()
		xqrg.addstring(string.format('%13s Hz %s',
		    ts(data.status.frequency), string.upper(data.status.mode)))
	end
end

return dataHandler
