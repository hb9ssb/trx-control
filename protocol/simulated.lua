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

-- Simulated transceiver protocol

local frequency = 14285000
local mode = 'usb'

local function initialize(driver)
	print (driver.name .. ': initialize')
end

local function lock(driver)
	print (driver.name .. ': locked')
end

local function unlock(driver)
	print (driver.name .. ': unlocked')
end

local function setFrequency(driver, freq)
	print (string.format('%s: set fequency to %s', driver.name, freq))
	frequency = freq
	return frequency
end

local function getFrequency(driver)
	print (driver.name .. ': get frequency')
	return frequency, mode
end

local function setMode(driver, mode)
	print (string.format('%s: set mode to %s', driver.name, mode))
	if driver.validModes[mode] ~= nil then
		mode = mode
		return mode
	else
		return 'invalid mode ' .. mode
	end
end

local function getMode(driver)
	print (driver.name .. ': get mode')
	return mode
end

return {
	name = 'simulated',
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
