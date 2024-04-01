-- Copyright (c) 2024 Derek Rowland NZ0P
-- Copyright (c) 2023 - 2024 Marc Balmer HB9SSB
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

-- Kenwood TS-480  protocol

-- Internal functions

local internalMode = {}

local function sendMessage(data)
	local message = data .. ';'
	trx.write(message)
end


-- Exported functions
local function initialize(driver)
	print (driver.name .. ': initialize')

	for k, v in pairs(driver.validModes) do
		internalMode[v] = k
	end
end

local function lock(driver)
	print (driver.name .. ': locked')
end

local function unlock(driver)
	print (driver.name .. ': unlocked')
end

local function setFrequency(driver, frequency)
	local data = string.format('FA%011d', frequency)
	sendMessage(data)
	return frequency
end

local function getFrequency(driver)
	sendMessage('FA')
	local reply = trx.read(14)
	local freq = string.sub(reply,3,13)
	
	sendMessage('MD')
        reply = trx.read(4)
        local mode = string.sub(reply,3,3)

	return tonumber(freq), internalMode[tonumber(mode)] or '??'
end


local function setMode(driver, band, mode)
	if driver.validModes[mode] == nil then
		return band, 'invalid mode' .. mode
	end

	local data = string.format('MD%d', driver.validModes[mode])
	sendMessage(data)

	return band, mode
end

local function getMode(driver)
	sendMessage('MD')
	local reply = trx.read(4)
        local mode = string.sub(reply,3,3)
	return internalMode[tonumber(mode)]
end

return {
	name = 'Kenwood TS-480 CAT protocol',
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
