-- Copyright (c) 2024 Derek Rowland NZ0P
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

-- trusdx CAT driver (based on Kenwood TS-480 command set)

local trusdx = require 'kenwood-ts480'
local config = ...

trusdx.validModes = {
	['lsb'] = 0x01,
	['usb'] = 0x02,
	['cw'] = 0x03,
	['fm'] = 0x04,
	['am'] = 0x05,
}

trusdx.filterSetting = {
	['50'] = 0x00,
	['100'] = 0x00,
	['200'] = 0x00,
	['500'] = 0x00,
	['1k8'] = 0x00,
	['2k4'] = 0x00,
	['3k0'] = 0x00,
	['4k0'] = 0x00
}

trusdx.name = 'trusdx'

trusdx.frequencyRange = {
	min = 30000,
	max = 100000000
}

return trusdx
