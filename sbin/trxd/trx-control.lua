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

-- Upper half of trx-control

local trxd = require 'trxd'

local driver = {}
local frequencyListeners = {}

local function registerDriver(newDriver)
	driver = newDriver

	if type(driver.initialize) == 'function' then
		driver.initialize()
	end
end

-- XXX drivers should be sandboxed
local function loadDriver(trxType)
	if trxType == nil then
		return 'command expects a trx-type'
	end

	if string.find(trxType, '/') ~= nil then
		return 'trx-type must not contain slashes'
	end

	local d = dofile(string.format('/usr/share/trxd/trx/%s.lua', trxType))
	if type(d) == 'table' then
		registerDriver(d)
		return 'new driver loaded'
	else
		return 'unexpected return value from driver'
	end
end

local function getTransceiver()
	return driver.transceiver or 'unknown transceiver'
end

local function setFrequency(freq)
	driver.setFrequency(freq)
	for k, v in ipairs(frequencyListeners) do
		trxd.send(v, string.format('frequency %s\n', freq))
	end
	return 'frequency set'
end

local function getFrequency()
	return driver.getFrequency()
end

local function listenFrequency(fd)
	print 'listen for frequency changes'
	frequencyListeners[#frequencyListeners + 1] = fd
	return 'listening'
end

local function unlistenFrequency(fd)
	print 'unlisten for frequency changes'

	for k, v in ipairs(frequencyListeners) do
		if v == fd then
			frequencyListeners[k] = nil
			return 'unlisten'
		end
	end
	return 'not listening'
end

return {
	registerDriver = registerDriver,
	loadDriver = loadDriver,
	getTransceiver = getTransceiver,
	setFrequency = setFrequency,
	getFrequency = getFrequency,
	listenFrequency = listenFrequency,
	unlistenFrequency = unlistenFrequency
}
