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

-- Yaesu FT-817 CAT driver

local ft817 = require 'cat-5-byte'

ft817.validModes = {
	lsb = 0x00,
	usb = 0x01,
	cw = 0x02,
	cwr = 0x03,
	am = 0x04,
	wfm = 0x06,
	fm = 0x08,
	dig = 0x0a,
	pkt = 0x0c
}

ft817.ctcssModes = {
	dcsOn = 0x0a,
	ctcssOn = 0x2a,
	encoderOn = 0x4a,
	off = 0x8a
}

ft817.name = 'Yaesu FT-817'

ft817.frequencyRange = {
	min = 100000,
	max = 450000000
}

return ft817
