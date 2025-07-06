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

	return { unknownCommand = command }
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
	trx.write(string.format('FA%09d;', tonumber(request.frequency)))
	response.frequency = request.frequency
end

local function getFrequency(driver, request, response)
	trx.write('FA;')
	local reply = trx.read(12)
	response.frequency = tonumber(string.sub(reply, 3, 11))
end

local function setMode(driver, request, response)
	local band = request.band
	local bcode

	response.band = band
	response.mode = request.mode

	if band == 'main' then
		bcode = '0'
	elseif band == 'sub' then
		bcode = '1'
	else
		response.status = 'Failure'
		response.reason = 'Missing or invalid band'
		return
	end

	if driver.validModes[mode] ~= nil then
		trx.write(string.format('MD%s%s;', bcode,
		    driver.validModes[mode]))
	else
		response.status = 'Failure'
		response.reason = 'Missing or invalid mode'
	end
end

local function getMode(driver, request, response, band)
	local band = request.band
	local bcode = '0'

	if band == 'sub' then
		bcode = '1'
	end

	trx.write(string.format('MD%s;', bcode))
	local reply = trx.read(5)
	local mode = string.sub(reply, 4, 4)

	for k, v in pairs(driver.validModes) do
		if v == mode then
			response.mode = k
			return
		end
	end
	response.status = 'Failure'
	response.reason = 'Unknown mode from trx'
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
	setMode = setMode
}
