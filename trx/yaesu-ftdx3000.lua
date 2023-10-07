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

-- The FTdx3000 CAT interface is very similar to the FT-710 CAT interface,
-- so we reuse that and only those functions that are different.

local ftdx3000 = require 'yaesu-ft-710'

ftdx3000.validModes = {
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

ftdx3000.initialize = function ()
	trx.setspeed(38400)
	trx.write('ID;')
	local reply = trx.read(7)

	if reply ~= 'ID0462;' then
		print 'this is not a Yaesu FTdx3000 transceiver'
	else
		print 'this is a Yaesu FTdx3000 transceiver'
	end
end

ftdx3000.setMode = function (band, mode)
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

ftdx3000.getMode = function (band)
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

return ftdx3000
