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

-- Yaesu FT-710 CAT driver

local mode = 'no mode set'

local validModes = {
	usb = true,
	lsb = true,
	cw = true,
	fm = true,
	am = true
}

local function initialize()
	print('ft-710: initialize')
	trx.setspeed(38400)
	trx.write('ID;')
	local reply = trx.read()
	if reply ~= 'ID0800;' then
		print 'this is not a Yaesu FT-710 transceiver'
	else
		print 'this is a Yaesu FT-710 transceiver'
	end
end

local function setFrequency(freq)
	print('ft-710: set frequency')
	trx.write(string.format('FA%s;', freq))
	return freq
end

local function getFrequency()
	print('ft-710: get frequency')
	trx.write('FA;')
	local reply = trx.read()
	return reply
end

local function setMode(mode)
	print (string.format('ft-710: set mode to %s', mode))
	if validModes[mode] ~= nil then
		mode = mode
		return mode
	else
		return 'invalid mode ' .. mode
	end
end

local function getMode()
	print 'ft-710: get mode'
	return mode
end

return {
	initialize = initialize,
	setFrequency = setFrequency,
	getFrequency = getFrequency,
	getMode = getMode,
	setMode = setMode
}
