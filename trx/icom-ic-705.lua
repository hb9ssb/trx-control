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

-- ICOM IC-705 CI-V driver

local ic705 = require 'ci-v'
local config, audio = ...

ic705.validModes = {
	['lsb'] = 0x00,
	['usb'] = 0x01,
	['am'] = 0x02,
	['cw'] = 0x03,
	['rtty'] = 0x04,
	['fm'] = 0x05,
	['wfm'] = 0x06,
	['cw-r'] = 0x07,
	['rtty-r'] = 0x08,
	['dv'] = 0x17
}

ic705.filterSetting = {
	['fil1'] = 0x01,
	['fil2'] = 0x02,
	['fil3'] = 0x03
}

ic705.name = 'ICOM IC-705'

ic705.frequencyRange = {
	min = 30000,
	max = 470000000
}

ic705.audio = audio

ic705.controllerAddress = config.controllerAddress or 0xe0
ic705.transceiverAddress = config.transceiverAddress or 0xa4

return ic705
