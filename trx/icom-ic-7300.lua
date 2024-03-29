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

-- ICOM IC-7300 CI-V driver

local ic7300 = require 'ci-v-7300'

ic7300.validModes = {
	['lsb'] = 0x00,
	['usb'] = 0x01,
	['am'] = 0x02,
	['cw'] = 0x03,
	['rtty'] = 0x04,
	['fm'] = 0x05,
	['cw-r'] = 0x07,
	['rtty-r'] = 0x08
}

ic7300.filterSetting = {
	['fil1'] = 0x01,
	['fil2'] = 0x02,
	['fil3'] = 0x03
}

ic7300.name = 'ICOM IC-7300'

ic7300.frequencyRange = {
	min = 30000,
	max = 74800000
}

return ic7300
