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

-- Yaesu 5-byte CAT protocol

local function initialize(driver)
	trx.setspeed(38400)
end

local function setLock(driver)
	trx.write('\x00\x00\x00\x00\x00')
	return 'locked'
end

local function setUnlock(driver)
	trx.write('\x00\x00\x00\x00\x80')
	return 'unlocked'
end

local function setFrequency(driver, frequency)
	local freq = string.sub(string.format('%09d', frequency), 1, -2)
	local bcd = trx.stringToBcd(freq)
	trx.write(bcd .. '\x01')
	return frequency
end

local function getFrequency(driver)
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
		return fn, mode
	else
		return nil
	end
end

local function setMode(driver, band, mode)
	local newMode = driver.validModes[mode]
	if newMode ~= nil then
		trx.write(string.format('%c\x00\x00\x00\x07', newMode))
		return band, mode
	else
		return band, 'invalid mode ' .. mode
	end
end

local function getMode(driver)
	trx.write('\x00\x00\x00\x00\x03')
	local f = trx.read(5)
	local m = string.byte(f, 5)

	local m = getMode()
	for k, v in pairs(driver.validModes) do
		if v == m then
			return k
		end
	end
	return 'unknown mode'
end

return {
	name = 'Yaesu 5-byte CAT protocol',
	validModes = {},
	ctcssModes = {},
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
	setMode = setMode
}
