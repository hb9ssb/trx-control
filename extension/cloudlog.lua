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
--
-- The trx-control cloudlog extension
--
-- Implements Cloudlog APIs: https://github.com/magicbug/Cloudlog/wiki/API
-- Implemented:
--   * /api/auth/<apiKey>
--   * /api/Radio/
-- Not Implemented (TODO):
--   * /api/station_info
--   * /api/QSO
--   * /api/statistics
--   * /api/logbook_check_callsign
--   * /api/logbook_check_grid
--
-- Implement in /etc/trxd.yaml (or configuration file):
-- extensions:
--  cloudlog:
--    script: cloudlog
--    configuration:
--      url: "https://cloudlog.xyz.com/index.php/api"
--      apiKey: "xxxxxxxxxxxx"
--
-- An example of this extension implemented in python can be used by a 
-- custom trx-control client can be found at: 
-- https://github.com/gx1400/python-trx-cloudlog

local config = ...
local curl = require 'curl'
local expat = require 'expat'

if trxd.verbose() > 0 then
        print 'installing the cloudlog extension'
end

if config.apiKey == nil then
	print 'cloudlog: api key is missing'
end

if config.url == nil then
	print 'cloudlog: url is missing'
else
	if trxd.verbose() > 0 then
		print('cloudlog: url ',config.url)
	end
end


local connectTimeout = config.connectTimeout or 3
local timeout = config.timeout or 15

-- local functions

local function apiAuth()
	local c = curl.easy()

	if trxd.verbose() > 0 then
		c:setopt(curl.OPT_VERBOSE, true)
	end

	c:setopt(curl.OPT_SSL_VERIFYHOST, 0)
	c:setopt(curl.OPT_SSL_VERIFYPEER, false)
	c:setopt(curl.OPT_CONNECTTIMEOUT, connectTimeout)
	c:setopt(curl.OPT_TIMEOUT, timeout)

	c:setopt(curl.OPT_URL, string.format('%s/auth/%s', config.url, config.apiKey))
	c:setopt(curl.OPT_HTTPGET, true)

	local t = {}
	c:setopt(curl.OPT_WRITEFUNCTION, function(a,b)
		table.insert(t, b)
		return #b
	end)

	local response = c:perform()
	local status = c:getinfo(curl.INFO_RESPONSE_CODE)
	c:cleanup()
	return response, table.concat(t), status
end

local function apiRadio(request)
	if request.frequency == nil then
		return false, "no frequency provided"
	end

	if request.mode == nil then
		return false, "no mode provided"
	end

	local c = curl.easy()
	
	if trxd.verbose() > 0 then
                c:setopt(curl.OPT_VERBOSE, true)
        end

	local radioname = request.radio or 'trx-control'

	local payload =	{
				key = config.apiKey,
				radio = radioname,
				frequency = request.frequency,
				mode = request.mode,
				timestamp = os.date('%Y/%m/%d %H:%M:%S')
			}

        c:setopt(curl.OPT_SSL_VERIFYHOST, 0)
        c:setopt(curl.OPT_SSL_VERIFYPEER, false)
        c:setopt(curl.OPT_CONNECTTIMEOUT, connectTimeout)
        c:setopt(curl.OPT_TIMEOUT, timeout)

        c:setopt(curl.OPT_URL, string.format('%s/Radio', config.url))
	c:setopt(curl.OPT_HTTPPOST, 1)
	c:setopt(curl.OPT_POSTFIELDS, json.encode(payload))

        local response = c:perform()
        local status = c:getinfo(curl.INFO_RESPONSE_CODE)
        c:cleanup()

        return response, status
end


-- public functions

function auth(request)
	local resp, data, status = apiAuth()

	if data ~= nil then
		local t = expat.decode(data)
		if t.auth.status then
			authStatus = t.auth.status.xmltext
			rights = t.auth.rights.xmltext
		else
			return {
				status = 'Error',
				reason = t.auth.message.xmltext
			}
		end
	end
	
	if resp == true and status == 200 then
		return {
			request = 'cloudlog',
			api = 'auth',
			status = 'Ok',
			auth = authStatus,
			rights = rights
		}
	else
		return {
			request = 'cloudlog',
			api = 'auth',
			status = 'Error',
			reason = 'Unable to authenticate'
		}
	end
end

function radio(request)
	resp, status = apiRadio(request)

	 if resp == true and status == 200 then
                return {
			request = 'cloudlog',
			api = 'radio',
			radio = request.radio,
                        status = 'Ok'
                }
        else
                return {
			request = 'cloudlog',
			api = 'radio',
                        status = 'Error',
                        reason = status
                }
        end

end

function verify(request)
        return { status = 'Ok', reply = 'cloudlog: extension running' }
end
