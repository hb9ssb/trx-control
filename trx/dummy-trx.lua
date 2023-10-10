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

-- Lower half of the trx-control Lua part

-- Dummy driver for development and testing purposes

local frequency = 14285000
local mode = 'usb'

local validModes = {
	usb = true,
	lsb = true,
	cw = true,
	fm = true,
	am = true
}

local function initialize()
	print 'dummy-trx: initialize'
end

local function lock()
	print 'dummy-trx: locked'
end

local function unlock()
	print 'dummy-trx: unlocked'
end

local function setFrequency(freq)
	print (string.format('dummy-trx: set fequency to %s', freq))
	frequency = freq
	return frequency
end

local function getFrequency()
	print 'dummy-trx: get frequency'
	return frequency, mode
end

local function setMode(mode)
	print (string.format('dummy-trx: set mode to %s', mode))
	if validModes[mode] ~= nil then
		mode = mode
		return mode
	else
		return 'invalid mode ' .. mode
	end
end

local function getMode()
	print 'dummy-trx: get mode'
	return mode
end

return {
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
