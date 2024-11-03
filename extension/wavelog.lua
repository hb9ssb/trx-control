-- Copyright (c) 2024 Fabian Berg HB9HIL
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
-- The trx-control Wavelog extension
--
-- Implements Wavelog APIs: https://github.com/wavelog/wavelog/wiki/API
-- Since Wavelog is a fork of Cloudlog, most API calls are compatible with
-- Cloudlog unless noted otherwise.
--
-- Implemented:
--   * /api/auth/<apiKey>
--   * /api/radio
--   * /api/qso
--   * /api/private_lookup (only wavelog, not implemented in cloudlog)
--   * /api/logbook_check_callsign (deprecated, use private_lookup instead)
--   * /api/logbook_check_grid
--   * /api/statistics
--   * /api/station_info
--   * /api/get_contacts_adif  (only wavelog, not implemented in cloudlog)
--
-- Implement in /etc/trxd.yaml (or configuration file):
-- extensions:
--  wavelog:
--    script: wavelog
--    configuration:
--      # URL of your wavelog instance
--      url: "https://wavelog.xyz.com/index.php"
--      # API key for your wavelog instance (use a r+w key)
--      apiKey: "xxxxxxxxxxxx"
--          # Use SSL (default: true)
--	    ssl: true
--

local config = ...

local curl = require 'curl'
local expat = require 'expat'
local log = require 'linux.sys.log'

if trxd.verbose() > 0 then
	log.syslog('notice', 'installing the wavelog extension')
	return
end

if config.apiKey == nil then
	log.syslog('err', 'wavelog: api key is missing')
	return
end

if config.ssl == nil then
	config.ssl = true
end

if config.url == nil then
	log.syslog('err', 'wavelog: url is missing')
	return
else
	if trxd.verbose() > 0 then
		log.syslog('notice', 'wavelog: url ' .. config.url)
	end
end

local connectTimeout = config.connectTimeout or 5
local timeout = config.timeout or 15

if config.ssl == true then
	verifyhost = 2
	ssl = true
else
	verifyhost = 0
	ssl = false
end

-- local functions

-- Generalized function for making cURL requests
local function apiRequest(url, payload, method, decode)
	local c = curl.easy()
	if decode == nil then decode = true end

	if trxd.verbose() > 0 then
		c:setopt(curl.OPT_VERBOSE, true)
	end

	c:setopt(curl.OPT_SSL_VERIFYHOST, verifyhost)
	c:setopt(curl.OPT_SSL_VERIFYPEER, ssl)
	c:setopt(curl.OPT_CONNECTTIMEOUT, connectTimeout)
	c:setopt(curl.OPT_TIMEOUT, timeout)

	if method == "GET" then
		c:setopt(curl.OPT_URL, url)
		c:setopt(curl.OPT_HTTPGET, true)
	else
		c:setopt(curl.OPT_URL, url)
		c:setopt(curl.OPT_HTTPPOST, 1)
		c:setopt(curl.OPT_POSTFIELDS, json.encode(payload))
	end

	local responseChunks = {}
	c:setopt(curl.OPT_WRITEFUNCTION, function(a, b)
		table.insert(responseChunks, b)
		return #b
	end)

	local response = c:perform()
	local status = c:getinfo(curl.INFO_RESPONSE_CODE)
	c:cleanup()

	if decode then
		return response, json.decode(table.concat(responseChunks)),
		    status
	else
		return response, table.concat(responseChunks), status
	end
end

-- API Functions

local function apiAuth()
	local url = string.format('%s/api/auth/%s', config.url, config.apiKey)
	return apiRequest(url, nil, "GET", false)
end

local function apiRadio(request)
	if not request.frequency then
		log.syslog('err', 'wavelog: no frequency provided')
		return false
	end
	if not request.mode then
		log.syslog('err', 'wavelog: no mode provided')
		return false
	end

	local url = string.format('%s/api/radio', config.url)
	local payload = {
		key = config.apiKey,
		radio = request.radio or 'trx-control',
		frequency = request.frequency,
		mode = request.mode,

		-- a timestamp is no longer necessary as it is set in wavelog
		-- but we keep it, just in case..
		timestamp = os.date('%Y/%m/%d %H:%M:%S')
	}

	if request.prop_mode ~= nil then payload.prop_mode = request.prop_mode end
	if request.sat_name ~= nil then payload.sat_name = request.sat_name end
	if request.power ~= nil then payload.power = request.power end
	if request.uplink_freq ~= nil then payload.uplink_freq = request.uplink_freq end
	if request.uplink_mode ~= nil then payload.uplink_mode = request.uplink_mode end
	if request.downlink_freq ~= nil then payload.downlink_freq = request.downlink_freq end
	if request.downlink_mode ~= nil then payload.downlink_mode = request.downlink_mode end
	if request.frequency_rx ~= nil then payload.frequency_rx = request.frequency_rx end
	if request.mode_rx ~= nil then payload.mode_rx = request.mode_rx end

	return apiRequest(url, payload, "POST")
end

local function apiQso(request)
	if not request.adif then
		log.syslog('err', 'wavelog: no adif data provided')
		return false
	end
	if not request.station_id then
		log.syslog('err', 'wavelog: no station profile id provided')
		return false
	end

	local url = string.format('%s/api/qso%s', config.url, request.dryrun and '/true' or '')
	local payload = {
		key = config.apiKey,
		type = 'adif',
		station_profile_id = request.station_id,
		string = request.adif
	}

	return apiRequest(url, payload, "POST")
end

local function apiPrivateLookup(request)
	if not request.callsign then
		log.syslog('err', 'wavelog: no callsign provided')
		return false
	end

	local url = string.format('%s/api/private_lookup', config.url)
	local payload = {
		key = config.apiKey,
		callsign = request.callsign
	}

	if request.station_ids ~= nil then payload.station_ids = request.station_ids end
	if request.band ~= nil then payload.band = request.band end
	if request.mode ~= nil then payload.mode = request.mode end

	return apiRequest(url, payload, "POST")
end

local function apiCallsignInLogbook(request)
	if not request.callsign then
		log.syslog('err', 'wavelog: no callsign provided')
		return false
	end
	if not request.slug then
		log.syslog('err', 'wavelog: no public logbook slug provided')
		return false
	end

	local url = string.format('%s/api/logbook_check_callsign', config.url)
	local payload = {
		key = config.apiKey,
		logbook_public_slug = request.slug,
		callsign = request.callsign
	}

	if request.band ~= nil then payload.band = request.band end

	return apiRequest(url, payload, "POST")
end

local function apiCheckGrid(request)
	if not request.grid then
		log.syslog('err', 'wavelog: no gridsquare provided')
		return false
	end
	if not request.slug then
		log.syslog('err', 'wavelog: no public logbook slug provided')
		return false
	end

	local url = string.format('%s/api/logbook_check_grid', config.url)
	local payload = {
		key = config.apiKey,
		logbook_public_slug = request.slug,
		grid = request.grid
	}

	if request.band ~= nil then payload.band = request.band end
	if request.cnfm ~= nil then payload.cnfm = request.cnfm end

	return apiRequest(url, payload, "POST")
end

local function apiStatistics()
	local url = string.format('%s/api/statistics/%s', config.url, config.apiKey)
	return apiRequest(url, nil, "GET")
end

local function apiStationInfo()
	local url = string.format('%s/api/station_info/%s', config.url, config.apiKey)
	return apiRequest(url, nil, "GET")
end

local function apiGetContactsAdif(request)
	if not request.station_id then
		log.syslog('err', 'wavelog: no station id provided')
		return false
	end
	if not request.fetchfromid then
		log.syslog('err', 'wavelog: no qso id provided from where to fetch (fetchfromid)')
		return false
	end

	local url = string.format('%s/api/get_contacts_adif', config.url)
	local payload = {
		key = config.apiKey,
		station_id = request.station_id,
		fetchfromid = request.fetchfromid
	}

	if request.limit ~= nil then payload.limit = request.cnlimitfm end

	return apiRequest(url, payload, "POST")
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
			request = 'wavelog',
			api = 'auth',
			status = 'Ok',
			auth = authStatus,
			rights = rights
		}
	else
		return {
			request = 'wavelog',
			api = 'auth',
			status = 'Error',
			reason = 'Unable to authenticate'
		}
	end
end

function radio(request)
	local resp, data, status = apiRadio(request)

	if resp and status == 200 then
        return {
			request = 'wavelog',
			api = 'radio',
			radio = request.radio,
			frequency = request.frequency,
			mode = request.mode,
            status = 'Ok'
        }
    else
        return {
			request = 'wavelog',
			api = 'radio',
            status = 'Error',
            reason = status
        }
    end
end

function qso(request)
	local resp, data, status = apiQso(request)

	if status == 201 then
		return {
			request = 'wavelog',
			api = 'qso',
			status = data.status,
			reason = data.reason or '',
			adif_count = data.adif_count,
			adif_errors = data.adif_errors,
			message = data.messages
		}
	else
		return {
			request = 'wavelog',
			api = 'qso',
			status = 'Error',
			reason = data
		}
	end
end

function private_lookup(request)
	local resp, data, status = apiPrivateLookup(request)

	if status == 200 then
		return {
			request = 'wavelog',
			api = 'private_lookup',
			status = 'Ok',
			callsign = data.callsign,
			dxcc = data.dxcc,
			dxcc_id = data.dxcc_id,
			dxcc_lat = data.dxcc_lat,
			dxcc_long = data.dxcc_long,
			dxcc_cqz = data.dxcc_cqz,
			dxcc_flag = data.dxcc_flag,
			cont = data.cont,
			name = data.name,
			gridsquare = data.gridsquare,
			location = data.location,
			iota_ref = data.iota_ref,
			state = data.state,
			us_county = data.us_county,
			qsl_manager = data.qsl_manager,
			bearing = data.bearing,
			call_worked = data.call_worked,
			call_worked_band = data.call_worked_band,
			call_worked_band_mode = data.call_worked_band_mode,
			lotw_member = data.lotw_member,
			dxcc_confirmed_on_band = data.dxcc_confirmed_on_band,
			dxcc_confirmed_on_band_mode = data.dxcc_confirmed_on_band_mode,
			dxcc_confirmed = data.dxcc_confirmed,
			call_confirmed = data.call_confirmed,
			call_confirmed_band = data.call_confirmed_band,
			call_confirmed_band_mode = data.call_confirmed_band_mode,
			suffix_slash = data.suffix_slash
		}
	else
		return {
			request = 'wavelog',
			api = 'private_lookup',
			status = 'Error',
			reason = data
		}
	end
end

function logbook_check_callsign(request)
	local resp, data, status = apiCallsignInLogbook(request)

	if status == 201 then
		return {
			request = 'wavelog',
			api = 'logbook_check_callsign',
			status = 'Ok',
			callsign = data.callsign,
			result = data.result
		}
	else
		return {
			request = 'wavelog',
			api = 'logbook_check_callsign',
			status = 'Error',
			reason = data
		}
	end
end

function logbook_check_grid(request)
	local resp, data, status = apiCheckGrid(request)

	if status == 201 then
		return {
			request = 'wavelog',
			api = 'logbook_check_grid',
			status = 'Ok',
			gridsquare = data.gridsquare,
			result = data.result
		}
	else
		return {
			request = 'wavelog',
			api = 'logbook_check_grid',
			status = 'Error',
			reason = data
		}
	end
end

function statistics(request)
	local resp, data, status = apiStatistics(request)

	if status == 201 then
		return {
			request = 'wavelog',
			api = 'statistics',
			status = 'Ok',
			today_qsos = data.Today,
			total_qsos = data.total_qsos,
			month_qsos = data.month_qsos,
			year_qsos = data.year_qsos
		}
	else
		return {
			request = 'wavelog',
			api = 'statistics',
			status = 'Error',
			reason = data
		}
	end
end

function station_info(request)
	local resp, data, status = apiStationInfo(request)

	if status == 200 then
		return {
			request = 'wavelog',
			api = 'station_info',
			status = 'Ok',
			result = data
		}
	else
		return {
			request = 'wavelog',
			api = 'station_info',
			status = 'Error',
			reason = data
		}
	end
end

function get_contacts_adif(request)
	local resp, data, status = apiGetContactsAdif(request)

	if status == 200 then
		local adif_cleaned = ""
		if type(data.adif) == "string" then
			adif_cleaned = data.adif:gsub("\r\n", " ")
		end
		return {
			request = 'wavelog',

			api = 'get_contacts_adif',
			status = 'Ok',
			message = data.message,
			lastfetchedid = data.lastfetchedid,
			exported_qsos = data.exported_qsos,
			adif = adif_cleaned
		}
	else
		return {
			request = 'wavelog',
			api = 'get_contacts_adif',
			status = 'Error',
			reason = data
		}
	end
end

function verify(request)
    return { status = 'Ok', reply = 'wavelog: extension running' }
end
