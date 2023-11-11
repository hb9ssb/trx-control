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

-- Lower half of the trx-control Lua part

-- Yaesu FT-897 CAT driver.  This driver is very similar to the FT-817 driver,
-- so we reuse that.  CTCSS/DCS mode setting and CTCSS tone setting as well
-- as Read TX state are different, though.

local ft897 = require 'cat-5-byte'

ft897.validModes = {
	lsb = 0x00,
	usb = 0x01,
	cw = 0x02,
	cwr = 0x03,
	am = 0x04,
	wfm = 0x06,
	fm = 0x08,
	dig = 0x0a,
	pkt = 0x0c,
	fmn = 0x88
}

ft897.ctcssModes = {
	dcsOn = 0x0a,
	dcsEncoderOn = 0x0b,
	ctcssOn = 0x2a,
	ctssEncoderOn = 0x3a,
	encoderOn = 0x4a,
	off = 0x8a
}

ft897.name = 'Yaesu FT-897'

return ft897
