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

local modeToInternalCode = {
	fm = '0',
	dv = '1',
	am = '2',
	lsb = '3',
	usb = '4',
	cw = '5',
	nfm = '6',
	dr = '7',
	wfm = '8',
	['r-cw'] = '9'
}
local internalCodeToMode = {}

local vfoToInternalCode = {
	['vfo-1'] = '0',
	['vfo-2'] = '1'
}
local internalCodeToVfo = {}
local lastVfo = 'vfo-1'

local internalState = {
	['0'] = 'off',
	['1'] = 'on'
}

local vfoMode = {
	['0'] = 'VFO mode',
	['1'] = 'Memory mode',
	['2'] = 'Call mode',
	['3'] = 'DV mode'
}

local internalStepSize = {
	['0'] = '5 kHz',
	['1'] = '6.25 kHz',
	['2'] = '8.33 kHz',
	['3'] = '9 kHz',
	['4'] = '10 kHz',
	['5'] = '12.5 kHz',
	['6'] = '15 kHz',
	['7'] = '20 kHz',
	['8'] = '25 kHz',
	['9'] = '30 kHz',
	['A'] = '50 kHz',
	['B'] = '100 Khz'
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
local function afGain(data)
	local gain = tonumber(string.sub(data, 4, 6))

	return { afGain = gain / 200 * 100 }
end

local function activeBand(data)
	local band = string.sub(data, 4, 4)

	return { activeBand = band == '0' and 'Band A' or 'Band B' }
end

local function squelch(data)
	local vfo = internalCodeToVfo[string.sub(data, 4, 4)]
	local state = internalState[string.sub(data, 6, 6)]

	return { ['squelch'] = { vfo = vfo, state = state } }
end

local function dualBand(data)
	local mode = string.sub(data, 4, 4)

	return { dualBandMode = mode == '0' and 'dualBand' or 'singleBand' }
end

local function frequency(data)
	local vfo = internalCodeToVfo[string.sub(data, 4, 4)]
	local hz = tonumber(string.sub(data, 6, -1))

	return { frequency = { vfo = vfo, hz = hz } }
end

local function mode(data)
	local vfo = internalCodeToVfo[string.sub(data, 4, 4)]
	local mode = internalCodeToMode[string.sub(data, 6, 6)]

	return { ['mode'] = { vfo = vfo, mode = mode } }
end

local function stepSize(data)
	local vfo = internalCodeToVfo[string.sub(data, 4, 4)]
	local stepSize = internalStepSize[string.sub(data, 6, 6)] or 'unknown'

	return { ['stepSize'] = { vfo = vfo, stepSize = stepSize } }
end

local function sMeterSquelch(data)
	local vfo = internalCodeToVfo[string.sub(data, 4, 4)]
	local state = internalState[string.sub(data, 6, 6)]

	return { ['S-meter squelch'] = { vfo = vfo, state = state } }
end

local function receive(data)
	return { receive = true }
end

local function transmit(data)
	local vfo = internalCodeToVfo[string.sub(data, 4, 4)]

	return { transmit = { vfo = vfo } }
end

local function vfoMode(data)
	local vfo = internalCodeToVfo[string.sub(data, 4, 4)]
	local mode = vfoMode[string.sub(data, 6, 6)]

	return { ['vfoMode'] = { vfo = vfo, mode = mode } }
end

local decoders = {
	AG = afGain,
	BC = activeBand,
	BY = squelch,
	DL = dualBand,
	FQ = frequency,
	MD = mode,
	SF = stepSize,
	SM = sMeterSquelch,
	RX = receive,
	TX = transmit,
	VM = vfoMode
}

local function handleStatusUpdates(driver, data)
	local command = string.sub(data, 1, 2)

	local decoder = decoders[command]
	if decoder ~= nil then
		local reply = decoder(data)
		return reply
	end

	return { unknownCommand = command }
end

-- Exported functions
local function initialize(driver)
	print (driver.name .. ': initialize')

	for k, v in pairs(modeToInternalCode) do
		internalCodeToMode[v] = k
	end

	for k, v in pairs(vfoToInternalCode) do
		internalCodeToVfo[v] = k
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
	local vfo = request.vfo or lastVfo

	local vfoCode = vfoToInternalCode[vfo]
	if vfoCode == nil then
		response.status = 'Failure'
		response.reason = 'Unknown VFO'
		return
	end
	response.frequency = request.frequency

	trx.write(string.format('FQ %s,%010d\r', vfoCode, request.frequency))
	response.vfo = vfo

	lastVfo = vfo
end

local function getFrequency(driver, request, response)
	local vfo = request.vfo or lastVfo

	local vfoCode = vfoToInternalCode[vfo]
	if vfoCode == nil then
		response.status = 'Failure'
		response.reason = 'Unknown VFO'
		return
	end

	trx.write(string.format('FQ %s\r', vfoCode))
	local reply = trx.read(16)
	local freq = string.sub(reply, 6, -1)
	response.frequency = tonumber(freq)

	trx.write(string.format('MD %s\r', vfoCode))
    reply = trx.read(7)
    local mode = string.sub(reply, 6, -2)

	response.mode = internalCodeToMode[mode] or 'unknown'

	response.vfo = vfo

	lastVfo = vfo
end

local function setMode(driver, request, response, band, mode)
	local vfo = request.vfo or lastVfo

	local vfoCode = vfoToInternalCode[vfo]
	if vfoCode == nil then
		response.status = 'Failure'
		response.reason = 'Unknown VFO'
		return
	end

	response.mode = request

	local code = modeToInternalCode[request.mode]

	if code == nil then
		response.status = 'Failure'
		response.reason = 'Unknown mode'
		return
	end

	trx.write(string.format('MD %s,%s\r', vfoCode, code))

	response.vfo = vfo
	lastVfo = vfo
end

local function getMode(driver, request, response)
	local vfo = request.vfo or lastVfo

	local vfoCode = vfoToInternalCode[vfo]
	if vfoCode == nil then
		response.status = 'Failure'
		response.reason = 'Unknown VFO'
		return
	end

	trx.write(string.format('MD %s\r', vfoCode))
	reply = trx.read(7)
    local mode = string.sub(reply, 6, -2)

	response.mode = internalCodeToMode[mode] or 'unknown'

	response.vfo = vfo
	lastVfo = vfo
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
