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

-- ICOM CI-V protocol

-- Internal functions
local function sendMessage(cn, sc, data)
	local message = '\xfe\xfe\xa4\xe0' .. cn

	if sc ~= nil then
		message = message .. sc
	end

	message = message .. data .. '\xfd'

	print('write message of ' .. #message .. ' bytes to trx')

	for n = 1, #message do
		io.write(string.format('%02x ', string.byte(message, n)))
	end
	print('')

	trx.write(message)
end

local function recvReply()
	local reply = trx.read(6)

	print('got reply: ')
	for n = 1, #reply do
		io.write(string.format('%02x ', string.byte(reply, n)))
	end
	print('')
	if string.byte(reply, 5) == 0xfb then
		return true
	else
		return false
	end
end

-- Exported functions
local function initialize(driver)
	print (driver.name .. ': initialize')
end

local function lock(driver)
	print (driver.name .. ': locked')
end

local function unlock(driver)
	print (driver.name .. ': unlocked')
end

local function setFrequency(driver, frequency)
	print (string.format('%s: set frequency to %s', driver.name, frequency))

	local freq = string.sub(string.format('%010d', frequency), 1, -1)
	print('freq', freq)
	local bcd = string.reverse(trx.stringToBcd(freq))

	for n = 1, #bcd do
		io.write(string.format('%02x ', string.byte(bcd, n)))
	end
	print('')
	print('bcd len:', #bcd)
	print('setting frequency')
	sendMessage('\x05', nil, bcd)
	if recvReply() == true then
		print 'frequency set'
	else
		print 'frequency not set'
	end
	return frequency
end

local function getFrequency(driver)
	print (driver.name .. ': get frequency')
	return 0, 'usb'
end

local function setMode(driver, mode)
	print (string.format('%s: set mode to %s', driver.name, mode))
	if driver.validModes[mode] ~= nil then
		return mode
	else
		return 'invalid mode ' .. mode
	end
end

local function getMode(driver)
	print (driver.name .. ': get mode')
	return 'usb'
end

return {
	name = 'ICOM CI-V',
	capabilities = {	-- driver specific
		frequency = true,
		mode = true,
		lock = true
	},
	validModes = {},
	ctcssModes = {},
	statusUpdatesRequirePolling = true,
	initialize = initialize,
	startStatusUpdates = nil,
	stopStatusUpdates = nil,
	handleStatusUpdates = nil,
	lock = lock,
	unlock = unlock,
	setFrequency = setFrequency,
	getFrequency = getFrequency,
	getMode = getMode,
	setMode = setMode
}
