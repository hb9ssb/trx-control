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

-- The keepalive extension for trx.control

local linux = require 'linux'

local config = ...

local timeout = config.timeout or 300

if trxd.verbose() > 0 then
	print 'initializing the trx-control keepalive extension'
	print('  - keepalive timeout is ' .. timeout .. ' seconds')
end

-- this script never returns, the extension must be marked as callable: false
-- in the config file.

while true do
	local keepalive = json.encode({
		status = 'Ok',
		request = 'keepalive'
	})

	linux.sleep(timeout)

	if trxd.verbose() > 0 then
		print 'sending keepalive package to clients'
	end
	trxd.notify(keepalive)
end
