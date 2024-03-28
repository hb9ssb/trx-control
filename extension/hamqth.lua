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

-- The hamqth.com callsign lookup extension for trx-control.

-- The HamQTH XML Interface Specification can be found at
-- https://www.hamqth.com/developers.php

local curl = require 'curl'
local expat = require 'expat'

local config = ...

if config.username == nil or config.password == nil then
	print 'hamqth: missing username and/or password'
	return
end

local connectTimeout = config.connectTimeout or 3
local timeout = config.timeout or 15
local url = config.url or 'https://www.hamqth.com'
local sessionId = {}
local callsignCache = {}

-- Request a session id
local function requestSessionId()
	local c = curl.easy()

	if trxd.verbose() > 0 then
		c:setopt(curl.OPT_VERBOSE, true)
	end
	c:setopt(curl.OPT_SSL_VERIFYHOST, 0)
	c:setopt(curl.OPT_SSL_VERIFYPEER, false)
	c:setopt(curl.OPT_CONNECTTIMEOUT, connectTimeout)
	c:setopt(curl.OPT_TIMEOUT, timeout)

	c:setopt(curl.OPT_URL,
	    string.format('%s/xml.php?u=%s&p=%s', url, config.username,
	    config.password))
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
	    string.format('%s/xml.php?id=%s&callsign=%s&prg=trxd-%s', url,
	    sessionId, callsign, trxd.version()))

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

local function getSessionId()
	local response, data, status = requestSessionId()
	if trxd.verbose() > 0 then
		print(data)
	end

	if response == true and status == 200 then
		local t = expat.decode(data)
		sessionId = t.HamQTH.session.session_id.xmltext or {}
	else
		sessionId = {}
	end
	print(type(sessionId))
	print(sessionId)
end

local function lookupCallsign(callsign)
	local response, data, status = requestCallsign(callsign)
	if trxd.verbose() > 0 then
		print(data)
	end
	if response == true and status == 200 then
		local t = expat.decode(data)
		if t.HamQTH.session ~= nil then
			local error = t.HamQTH.session.error
			if error ~= nil then
				return nil, error.xmltext
			end
		end
		local callsign = t.HamQTH.search
		return {
			callsign = callsign.callsign.xmltext or '',
			nick = callsign.nick.xmltext or '',
			qth = callsign.qth.xmltext or '',
			country = callsign.country.xmltext or '',
			adif = callsign.adif.xmltext or '',
			itu = callsign.itu.xmltext or '',
			cq = callsign.cq.xmltext or '',
			grid = callsign.grid.xmltext or '',
			adr_city = callsign.adr_city.xmltext or '',
			adr_zip = callsign.adr_zip.xmltext or '',
			adr_country = callsign.adr_country.xmltext or '',
			adr_adif = callsign.adr_adif.xmltext or '',
			lotw = callsign.lotw.xmltext or '',
			qsldirect = callsign.qsldirect.xmltext or '',
			qsl = callsign.qsl.xmltext or '',
			eqsl = callsign.eqsl.xmltext or '',
			email = callsign.email.xmltext or '',
			latitude = callsign.latitude.xmltext or '',
			longitude = callsign.longitude.xmltext or '',
			continent = callsign.continent.xmltext or '',
			utc_offset = callsign.utc_offset.xmltext or '',
			picture = callsign.picture.xmltext or ''
		}, nil
	end
	return nil, status
end

function lookup(request)
	if request.callsign == nil then
		return {
			status = 'Error',
			response = 'lookup',
			reason = 'No callsign'
		}
	end

	local callsign = string.lower(request.callsign)

	local data = callsignCache[callsign]

	if data ~= nil then
		return {
			status = 'Ok',
			response = 'lookup',
			source = 'cache',
			data = data
		}
	end

	if #sessionId == 0 then
		getSessionId()

		if #sessionId == 0 then
			return {
				status = 'Error',
				response = 'lookup',
				reason = 'Unable to get session id'
			}
		end
	end

	local data, error = lookupCallsign(callsign)
	if data == nil and error ~= nil then
		getSessionId()

		if #sessionId == 0 then
			return {
				status = 'Error',
				response = 'lookup',
				reason = 'Unable to get session id'
			}
		end

		data, error = lookupCallsign(callsign)
	end

	if data ~= nil then
		callsignCache[callsign] = data
		return {
			status = 'Ok',
			response = 'lookup',
			source = 'hamqth',
			data = data
		}
	else
		return {
			status = 'Error',
			response = 'lookup',
			reason = 'Unable to lookup callsign: ' .. error
		}
	end
end
