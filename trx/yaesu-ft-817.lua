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

-- Yaesu FT-817 CAT driver

local mode = 'no mode set'

local validModes = {
	usb = true,
	lsb = true,
	cw = true,
	fm = true,
	am = true
}

local function initialize()
	trx.setspeed(38400)
end

local function setFrequency(frequency)
	local freq = string.sub(string.format('%09d', frequency), 1, -2)
	print('set frequency to ' ..  freq)
	local bcd = trx.stringToBcd(freq)
	trx.write(bcd .. '\x01')
	return frequency
end

local function getFrequency()
	trx.write('\x00\x00\x00\x00\x03')
	local f = trx.read(5)
	frequency = trx.bcdToString(string.sub(f, 1, 4))
	return frequency
end

local function setMode(mode)
	print (string.format('ft-817: set mode to %s', mode))
	if validModes[mode] ~= nil then
		mode = mode
		return mode
	else
		return 'invalid mode ' .. mode
	end
end

local function getMode()
	print 'ft-817: get mode'
	return mode
end


return {
	statusUpdatesRequirePolling = true,
	initialize = initialize,
	setFrequency = setFrequency,
	getFrequency = getFrequency,
	getMode = getMode,
	setMode = setMode
}
