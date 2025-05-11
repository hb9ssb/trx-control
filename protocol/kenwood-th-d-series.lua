-- Copyright (c) 2025 Marc Balmer HB9SSB
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

-- Kenwood TH-Dxx series protocol

-- Internal functions

local internalMode = {}

local internalBand = {
	['0'] = 'A',
	['1'] = 'B'
}

local internalState = {
	['0'] = 'off',
	['1'] = 'on'
}

-- Handling auto information

local function startStatusUpdates(driver)
	print('start status updates')
	trx.write('AI 1\r')
	local reply = trx.read(5)
	print('reply:', reply)
	return string.byte('\r')
end

local function stopStatusUpdates(driver)
	print('stop status updates')
	trx.write('AI 0\r')
	local reply = trx.read(5)
	print('reply:', reply)
	return string.byte('\r')
end

-- Decoders for auto information
local function squelch(data)
	local band = internalBand[string.sub(data, 4, 4)]
	local state = internalState[string.sub(data, 6, 6)]

	return { ['squelch'] = { band = band, state = state } }
end

local function frequency(data)
	local band = internalBand[string.sub(data, 4, 4)]
	local hz = tonumber(string.sub(data, 6, -1))

	return { frequency = { band = band, hz = hz } }
end

local function sMeterSquelch(data)
	local band = internalBand[string.sub(data, 4, 4)]
	local state = internalState[string.sub(data, 6, 6)]

	return { ['S-meter squelch'] = { band = band, state = state } }
end

local decoders = {
	BY = squelch,
	FQ = frequency,
	SM = sMeterSquelch,
}

local function handleStatusUpdates(driver, data)
	print('status update, got:', data)
	local command = string.sub(data, 1, 2)

	local decoder = decoders[command]
	if decoder ~= nil then
		local reply = decoder(data)
		print(reply.lock)
		return reply
	end

	return { unknownCommand = command }
end

-- Exported functions
local function initialize(driver)
	print (driver.name .. ': initialize')

	for k, v in pairs(driver.validModes) do
		internalMode[v] = k
	end
end

local function setLock(driver, request, response)
	print (driver.name .. ': locked')
	response.state = 'unlocked'
end

local function setUnlock(driver, request, response)
	print (driver.name .. ': unlocked')
	response.state = 'unlocked'
end

local function setFrequency(driver, request, response)
	response.frequency = request.frequency

	local data = string.format('FA%011d', request.frequency)
	sendMessage(data)
end

local function getFrequency(driver, request, response)
	trx.write('FQ 0\r')
	local reply = trx.read(16)
	local freq = string.sub(reply, 6, -1)
	response.frequency = tonumber(freq)

	trx.write('MD 0\r')
        reply = trx.read(7)
        local mode = string.sub(reply, 6, -2)

	response.mode = internalMode[mode] or '??'
end

local function setMode(driver, request, response, band, mode)

	response.mode = request

	if driver.validModes[mode] == nil then
		response.status = 'Failure'
		response.reason = 'Invalid mode'
		return
	end

	trx.write(string.format('MD 0,%s\r', driver.validModes[mode]))
end

local function getMode(driver, request, response)
	trx.write('MD 0\r')
	reply = trx.read(7)
        local mode = string.sub(reply, 6, -2)

	response.mode = '??'
	for k, v in pairs(driver.validModes) do
		if v == mode then
			response.mode = k
			break
		end
	end
end

return {
	name = 'Kenwood TH-D series CAT protocol',
	capabilities = {	-- driver specific
		frequency = true,
		mode = true,
		lock = true
	},
	validModes = {},
	ctcssModes = {},
	statusUpdatesRequirePolling = false,
	initialize = initialize,
	startStatusUpdates = startStatusUpdates,
	stopStatusUpdates = stopStatusUpdates,
	handleStatusUpdates = handleStatusUpdates,
	setLock = setLock,
	setUnlock = setUnlock,
	setFrequency = setFrequency,
	getFrequency = getFrequency,
	getMode = getMode,
	setMode = setMode
}
