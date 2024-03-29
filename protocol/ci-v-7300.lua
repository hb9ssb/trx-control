-- Copyright (c) 2024 Derek Rowland NZ0P
-- Copyright (c) 2024 Marc Balmer HB9SSB
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

local internalMode = {}
local internalFilter = {}

local function sendMessage(cn, sc, data)
	local addr =  '\x94'
	local message = '\xfe\xfe' .. addr .. '\xe0' .. cn

	if sc ~= nil then
		message = message .. sc
	end

	if data ~= nil then
		message = message .. data
	end

	message = message .. '\xfd'

	trx.write(message)
end

local function recvReply()
        local reply = trx.read(6)

        if string.byte(reply, 6) == 0xfd then
                return true
        else
                return false
        end
end

-- Exported functions
local function initialize(driver)
	print (driver.name .. ': initialize')

	for k, v in pairs(driver.validModes) do
		internalMode[v] = k
	end

	for k, v in pairs(driver.filterSetting) do
		internalFilter[v] = k
	end
end

local function lock(driver)
	print (driver.name .. ': locked')
end

local function unlock(driver)
	print (driver.name .. ': unlocked')
end

local function setFrequency(driver, frequency)

	local freq = string.sub(string.format('%010d', frequency), 1, -1)
	local bcd = string.reverse(trx.stringToBcd(freq))

	sendMessage('\x05', nil, bcd)
	if recvReply() == true then
		print 'frequency set'
	else
		print 'frequency not set'
	end
	return frequency
end

local function getFrequency(driver)
	sendMessage('\x03')
	recvReply()
	local reply = trx.read(10)
	local freq = trx.bcdToString(string.reverse(string.sub(reply, 6, -2)))

	sendMessage('\x04')
	recvReply()
	reply = trx.read(7)

	local mode = string.byte(reply, 6)
	local fil = string.byte(reply, 7)

	return tonumber(freq), internalMode[mode] or '??' --, internalFilter[fil] or '??'
end

local function setMode(driver, band, mode)
	if driver.validModes[mode] ~= nil then
		local payload = string.char(driver.validModes[mode])
		sendMessage('\x01', nil, payload)
		recvReply()
		return band, mode
	else
		return band, 'invalid mode ' .. mode
	end
end

local function getMode(driver)
	sendMessage('\x04')
	recvReply()
	local reply = trx.read(7)

	local mode = string.byte(reply, 6)

	return internalMode[mode] or '??'
end

return {
	name = 'ICOM CI-V',
	capabilities = {	-- driver specific
		frequency = true,
		mode = true,
		lock = true
	},
	addrCiV = nil,
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
