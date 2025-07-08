-- Copyright (c) 2023 - 2025 Marc Balmer HB9SSB
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

-- Yaesu 5-byte CAT protocol

local modeToInternalCode = {
	lsb = 0x00,
	usb = 0x01,
	cw = 0x02,
	cwr = 0x03,
	am = 0x04,
	wfm = 0x06,
	fm = 0x08,
	dig = 0x0a,
	pkt = 0x0c,
	fmn = 0x88
}

local function initialize(driver, functions)
	trx.read(1)
	if driver.powerControl == false then
		functions['power-on'] = nil
		functions['power-off'] = nil
	end
end

local function setLock(driver, request, response)
	trx.write('\x00\x00\x00\x00\x00')
	response.state = 'locked'
end

local function setUnlock(driver, request, response)
	trx.write('\x00\x00\x00\x00\x80')
	response.state = 'unlocked'
end

local function setFrequency(driver, request, response)
	local freq = string.sub(string.format('%09d', request.frequency), 1, -2)
	local bcd = trx.stringToBcd(freq)
	trx.write(bcd .. '\x01')
	response.frequency = request.frequency
end

local function getFrequency(driver, request, response)
	trx.write('\x00\x00\x00\x00\x03')
	local f = trx.read(5)
	if f ~= nil then
		local frequency = trx.bcdToString(string.sub(f, 1, 4))
		local modeCode = string.byte(string.sub(f, 5))
		local mode = ''
		for k, v in pairs(driver.validModes) do
			if v == modeCode then
				mode = k
			end
		end
		if #mode == 0 then
			mode = nil
		end
		local fn = tonumber(frequency) * 10
		response.frequency = fn
		response.mode = mode
	else
		response.status = 'Failure'
		response.reason = 'No reply from trx'
	end
end

local function setMode(driver, request, response)
	local newMode = driver.validModes[request.mode]

	response.mode = mode

	if newMode ~= nil then
		trx.write(string.format('%c\x00\x00\x00\x07', newMode))
	else
		response.status = 'Failure'
		response.reason = 'Unknown mode'
	end
end

local function getMode(driver, request, response)
	trx.write('\x00\x00\x00\x00\x03')
	local f = trx.read(5)
	local m = string.byte(f, 5)

	for k, v in pairs(driver.validModes) do
		if v == m then
			response.mode = k
			return
		end
	end

	response.status = 'Failure'
	response.reason = string.format('Unknown mode code %X', m)
end

local function powerOn(driver, request, response)
	trx.write('\x00\x00\x00\x00\x00')
	trx.write('\x00\x00\x00\x00\x0f')
end

local function powerOff(driver, request, response)
		trx.write('\x00\x00\x00\x00\x8f')
end

local function getPtt(driver, request, response)
	trx.write('\x00\x00\x00\x00\xf7')
	local status = trx.read(1)

	response.raw = string.format('%02x', string.byte(status))
	if string.byte(status) & 0x80 == 0x80 then
		response.ptt = 'off'
	else
		response.ptt = 'on'
	end
end

local function setPtt(driver, request, response)

	if request.ptt == 'on' then
		trx.write('\x00\x00\x00\x00\x08')
	elseif request.ptt == 'off' then
		trx.write('\x00\x00\x00\x00\x88')
	else
		response.status = 'Failure'
		response.reason = 'Unkknown PTT mode'
		return
	end

	response.ptt = request.ptt
end

return {
	name = 'Yaesu 5-byte CAT protocol',
	capabilities = {	-- driver specific
		frequency = true,
		mode = true,
		lock = true
	},
	validModes = {},	-- trx specific
	ctcssModes = {},	-- trx specific
	statusUpdatesRequirePolling = true,
	initialize = initialize,
	startStatusUpdates = nil,
	stopStatusUpdates = nil,
	handleStatusUpdates = nil,
	setLock = setLock,
	setUnlock = setUnlock,
	setFrequency = setFrequency,
	getFrequency = getFrequency,
	getMode = getMode,
	setMode = setMode,
	getPtt = getPtt,
	setPtt = setPtt,
	powerOn = powerOn,
	powerOff = powerOff,
}
