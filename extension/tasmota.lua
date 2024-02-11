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

-- The tasmota for trx-control.  This is realised as a simple extension, but
-- could as well be a GPIO.

local curl = require 'curl'
local config = ...

local connectTimeout = config.connectTimeout or 3
local timeout = config.timeout or 15
local address = config.address
local adminPassword = config.adminPassword

if trxd.verbose() > 0 then
	print 'installing the trx-control tasmota extension'
end

local function requestState(state)
	local c = curl.easy()

	if trxd.verbose() > 0 then
		c:setopt(curl.OPT_VERBOSE, true)
	end
	c:setopt(curl.OPT_SSL_VERIFYHOST, 0)
	c:setopt(curl.OPT_SSL_VERIFYPEER, false)
	c:setopt(curl.OPT_CONNECTTIMEOUT, connectTimeout)
	c:setopt(curl.OPT_TIMEOUT, timeout)
	c:setopt(curl.OPT_HTTPGET, true)
	c:setopt(curl.OPT_URL,
	    string.format('http://%s/cm?cmnd=Power%%20%s', address, state))

	local t = {}
	c:setopt(curl.OPT_WRITEFUNCTION, function (a, b)
		table.insert(t, b)
		return #b
	end)

	r = c:perform()
	local status = c:getinfo(curl.INFO_RESPONSE_CODE)
	c:cleanup()

	return r, table.concat(t), status
end


function power(request)
	local state = string.lower(request.state or 'state')

	local status, reply = requestState(state)

	if status == true then
		local data = json.decode(reply)

		if data ~= nil then
			return {
				status = 'Ok',
				power = string.lower(data.POWER)
			}
		else
			return {
				status = 'Error',
				reason = 'Unable set or get power state'
			}
		end
	else
		return {
			status = 'Error',
			reason = 'Unable to connect tasmota device'
		}
	end
end
