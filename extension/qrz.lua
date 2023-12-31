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

-- The QRZ.COM callsign lookup extension for trx-control

local curl = require 'curl'
local expat = require 'expat'

local config = ...

if config.username == nil or config.password == nil then
	print 'qrz: missing username and/or password'
	return
end

local connectTimeout = config.connectTimeout or 3
local timeout = config.timeout or 15
local url = config.url or 'https://xmldata.qrz.com'
local sessionKey = {}
local callsignCache = {}

-- Request a session key
local function requestSessionKey()
	local c = curl.easy()

	if trxd.verbose() > 0 then
		c:setopt(curl.OPT_VERBOSE, true)
	end
	c:setopt(curl.OPT_SSL_VERIFYHOST, 0)
	c:setopt(curl.OPT_SSL_VERIFYPEER, false)
	c:setopt(curl.OPT_CONNECTTIMEOUT, connectTimeout)
	c:setopt(curl.OPT_TIMEOUT, timeout)

	c:setopt(curl.OPT_URL,
	    string.format('%s/xml/current/?username=%s&password=%s&'
	    .. 'agent=trx-control', url, config.username, config.password))
	c:setopt(curl.OPT_HTTPGET, true)

	local t = {}
	c:setopt(curl.OPT_WRITEFUNCTION, function (a, b)
		table.insert(t, b)
		return #b
	end)

	local r = c:perform()
	local response = c:getinfo(curl.INFO_RESPONSE_CODE)
	c:cleanup()

	return r, table.concat(t), response
end

local function requestCallsign(callsign)
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
	    string.format('%s/xml/current/?s=%s&callsign=%s', url, sessionKey,
	    callsign))

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

local function getSessionKey()
	local response, data, status = requestSessionKey()
	if trxd.verbose() > 0 then
		print(data)
	end

	if response == true and status == 200 then
		local t = expat.decode(data)
		sessionKey = t.QRZDatabase.Session.Key.xmltext or {}
	else
		sessionKey = {}
	end
	print(type(sessionKey))
	print(sessionKey)
end

local function lookupCallsign(callsign)
	local response, data, status = requestCallsign(callsign)
	if trxd.verbose() > 0 then
		print(data)
	end
	if response == true and status == 200 then
		local t = expat.decode(data)
		local error = t.QRZDatabase.Session.Error
		if error ~= nil then
			return nil, error.xmltext
		end
		local callsign = t.QRZDatabase.Callsign
		return {
			call = callsign.call or '',
			name = callsign.name or '',
			fname = callsign.fname or '',
			addr2 = callsign.addr2 or '',
			country = callsign.country or ''
		}, nil
	end
	return nil, status
end

function lookup(request)
	if request.callsign == nil then
		return {
			status = 'Error',
			reason = 'No callsign'
		}
	end

	local callsign = string.lower(request.callsign)

	local data = callsignCache[callsign]

	if data ~= nil then
		return {
			status = 'Ok',
			source = 'cache',
			data = data
		}
	end

	if #sessionKey == 0 then
		getSessionKey()

		if #sessionKey == 0 then
			return {
				status = 'Error',
				reason = 'Unable to get session key'
			}
		end
	end

	local data, error = lookupCallsign(callsign)
	if data == nil and error ~= nil then
		getSessionKey()

		if #sessionKey == 0 then
			return {
				status = 'Error',
				reason = 'Unable to get session key'
			}
		end

		data, error = lookupCallsign(callsign)
	end

	if data ~= nil then
		callsignCache[callsign] = data
		return {
			status = 'Ok',
			source = 'qrz',
			data = data
		}
	else
		return {
			status = 'Error',
			reason = 'Unable to lookup callsign: ' .. error
		}
	end
end
