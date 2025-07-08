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

-- Yaesu character delimited CAT protocol

local modeToInternalCode = {
	lsb = '1',
	usb = '2',
	['cw-u'] = '3',
	fm = '4',
	am = '5',
	['rtty-l'] = '6',
	['cw-l'] = '7',
	['data-l'] = '8',
	['rtty-u'] = '9',
	['data-fm'] = 'A',
	['fm-n'] = 'B',
	['data-u'] = 'C',
	['am-n'] = 'D',
	psk = 'E',
	['data-fm-n'] = 'F'
}
local internalCodeToMode = {}

local vfoToInternalCode = {
	['vfo-1'] = '0',
	['vfo-2'] = '1'
}
local internalCodeToVfo = {}
local lastVfo = 'vfo-1'

local function initialize(driver)
	trx.read(1)
	trx.write('ID;')
	local reply = trx.read(7)

	if reply ~= nil then
		if trx.verbose() > 0 then
			print(string.format('transceiver ID (%s): %s', reply, reply:sub(3, -2)))
		end

		if reply ~= 'ID' .. driver.ID .. ';' then
			print ('this is not a ' .. driver.name .. ' transceiver')
		else
			print ('this is a ' .. driver.name .. ' transceiver')
		end
	end

	for k, v in pairs(modeToInternalCode) do
		internalCodeToMode[v] = k
	end

	for k, v in pairs(vfoToInternalCode) do
		internalCodeToVfo[v] = k
	end

end

-- Handling auto information
local function startStatusUpdates(driver)
	trx.write('AI1;')
	return string.byte(';')
end

local function stopStatusUpdates(driver)
	trx.write('AI0;')
	return 'status updates off'
end

-- Decoders for auto information
local function afGain(data)
	print('afGain', data)
	local gain = tonumber(string.sub(data, 4, 6))

	return { afGain = string.format('%.f', gain / 255 * 100) }
end

local function frequencyVfoA(data)
	local hz = tonumber(string.sub(data, 3, 11))
	return { vfo = 'vfo-a', frequency = hz }
end

local function frequencyVfoB(data)
	local hz = tonumber(string.sub(data, 3, 11))
	return { vfo = 'vfo-b', frequency = hz }
end

local function informationVfoA(data)
	mode = string.sub(data, 3, 5)
	local hz = tonumber(string.sub(data, 6, 14))

	return {
		vfo = 'vfo-a',
		mode = mode,
		frequency = hz
	}
end

local function lockDecoder(data)
	if string.sub(data, 3, 3) == '0' then
		print('lock off')
		return { lock = 'off' }
	else
		print 'lock on'
		return { lock = 'on' }
	end
end

local function mode(data)
	local band = string.sub(data, 3, 3)
	local mode = string.sub(data, 4, 4)

	if band == '0' then
		band = 'main'
	else
		band = 'sub'
	end
	local name = 'unknown mode'

	for k, v in pairs(validModes) do
		if v == mode then
			name = k
			break
		end
	end

	return {
		operatingMode = {
			band = band,
			mode = name
		}
	}
end

local decoders = {
	AG = afGain,
	FA = frequencyVfoA,
	FB = frequencyVfoB,
	IF = informationVfoA,
	LK = lockDecoder,
	MD = mode
}

local function handleStatusUpdates(driver, data)
	local command = string.sub(data, 1, 2)
	local decoder = decoders[command]
	if decoder ~= nil then
		local reply = decoder(data)
		print(reply.lock)
		return reply
	end

	return { unknownCommand = command, ['raw-data'] = data }
end

-- direct driver commands

local function setLock(driver, request, response)
	trx.write('LK1;')
	response.state = 'locked'
end

local function setUnlock(driver, request, response)
	trx.write('LK0;')
	response.state = 'unlocked'
end

local function setFrequency(driver, request, response)
	local vfo = request.vfo or lastVfo

	if vfo == 'vfo-1' then
		trx.write(string.format('FA%09d;', tonumber(request.frequency)))
	elseif vfo == 'vfo-2' then
		trx.write(string.format('FB%09d;', tonumber(request.frequency)))
	else
		response.status = 'Failure'
		response.reason = 'Unknown VFO'
		return
	end

	response.vfo = vfo
	response.frequency = request.frequency
end

local function getFrequency(driver, request, response)
	local vfo = request.vfo or lastVfo

	if vfo == 'vfo-1' then
		trx.write('FA;')
	elseif vfo == 'vfo-2' then
		trx.write('FB;')
	else
		response.status = 'Failure'
		response.reason = 'Unknown VFO'
		return
	end

	local reply = trx.read(12)
	response.vfo = vfo
	response.frequency = tonumber(string.sub(reply, 3, 11))
end

local function setMode(driver, request, response)
	local vfo = request.vfo or lastVfo

	response.vfo = vfo
	response.mode = request.mode

	local vfoCode = vfoToInternalCode[vfo] or '0'

	trx.write(string.format('MD%s%s;', vfoCode, modeToInternalCode[request.mode]))

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

	trx.write(string.format('MD%s;', vfoCode))
	local reply = trx.read(5)
	local mode = string.sub(reply, 4, 4)

	response.mode = internalCodeToMode[mode] or 'unknown'

	response.vfo = vfo
	lastVfo = vfo
end

local function powerOn(driver, request, response)
	trx.write('PS1;')
end

local function powerOff(driver, request, response)
	trx.write('PS0;')
end

local function getPtt(driver, request, response)
	trx.write('TX;')
	local reply = trx.read(4)

	local code = string.sub(reply, 3, 3)

	if code == '0' then
		response.ptt = 'off'
	else
		response.ptt = 'on'
	end
end

local function setPtt(driver, request, response)

	if request.ptt == 'on' then
		trx.write('TX1;')
	elseif request.ptt == 'off' then
		trx.write('TX0;')
	else
		response.status = 'Failure'
		response.reason = 'Unkknown PTT mode'
		return
	end

	response.ptt = request.ptt
end

return {
	name = 'Yaesu character delimited CAT protocol',
	ID = '0000',
	capabilities = {	-- driver specific
		frequency = true,
		mode = true,
		lock = true
	},
	validModes = {},
	ctcssModes = {},
	initialize = initialize,
	startStatusUpdates = startStatusUpdates,
	stopStatusUpdates = stopStatusUpdates,
	handleStatusUpdates = handleStatusUpdates,
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
