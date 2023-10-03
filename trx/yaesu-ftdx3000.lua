-- Copyright (c) 2023 Marc Balmer HB9SSB
--               2023 Michael Grindle KA7Y    
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

-- Yaesu FTdx3000 CAT driver

local validModes = {
	['lsb'] = '1',
	['usb'] = '2',
	['cw'] = '3',
	['fm'] = '4',
	['am'] = '5',
	['rtty-l'] = '6',
	['cw-r'] = '7',
	['pkt-l'] = '8',
	['rtty-u'] = '9',
	['pkt-fm'] = 'A',
	['fm-n'] = 'B',
	['pkt-u'] = 'C',
	['am-n'] = 'D',
}

local function initialize()
	trx.setspeed(38400)
	trx.write('ID;')
	local reply = trx.read(7)

--
-- not sure how to determine this "ID" number
--

	if reply ~= 'ID0800;' then
		print 'this is not a Yaesu FTdx3000 transceiver'
	else
		print 'this is a Yaesu FTdx3000 transceiver'
	end
end

local function startStatusUpdates()
	trx.write('AI1;')
	return string.byte(';')
end

local function stopStatusUpdates()
	trx.write('AI0;')
	return 'status updates off'
end

local function handleStatusUpdates(data)
	print('FTdx3000: handle status update, data:', data)
	return {
		rawData = data
	}
end

-- There are 8 different lock codes for combinations of 2 VFOs
-- for the FTdx3000. Codes: LK0 thru LK7
--  How would I add the capability to pick 1 of the 8 possibilities.
--  LK0 and LK1 only unlock and lock VFO-A

local function lock()
	trx.write('LK1;')
	return 'locked'
end

local function unlock()
	trx.write('LK0;')
	return 'unlocked'
end

-- Need to set and get Frequency for VFO-B also

local function setFrequency(freq)
	trx.write(string.format('FA%s;', freq))
	return freq
end

local function getFrequency()
	trx.write('FA;')
	local reply = trx.read(12)
	return tonumber(string.sub(reply, 3, 11))
end

local function setMode(band, mode)
	print('FTdx3000', band, mode)
	local bcode = '0'
-- FTdx3000 does not use main/sub. The first character (bcode)
--  is always a value of zero.
--[[
	if band == 'main' then
		bcode = '0'
	elseif band == 'sub' then
		bcode = '1'
	else
		return nil, 'invalid band'
	end
--]]

	if validModes[mode] ~= nil then
		trx.write(string.format('MD%s%s;', bcode, validModes[mode]))
		return band, mode
	else
		return nil, 'invalid mode'
	end
end

local function getMode(band)
	local bcode = '0'
-- FTdx3000 does not use main/sub
--[[
	if band == 'sub' then
		bcode = '1'
	end
--]]

	trx.write(string.format('MD%s;', bcode))
	local reply = trx.read(5)
	local mode = string.sub(reply, 4, 4)

	for k, v in pairs(validModes) do
		if v == mode then
			return k
		end
	end
	return 'unknown mode'
end

return {
	initialize = initialize,
	startStatusUpdates = startStatusUpdates,
	stopStatusUpdates = stopStatusUpdates,
	handleStatusUpdates = handleStatusUpdates,
	lock = lock,
	unlock = unlock,
	setFrequency = setFrequency,
	getFrequency = getFrequency,
	getMode = getMode,
	setMode = setMode
}
