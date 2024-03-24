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

-- The logbook extension for trx-control provides a hamradio logbook that
-- store all data in a PostgreSQL database.

local pgsql = require 'pgsql'

-- The configuration is stored in trxd.yaml under the extension.  The following
-- configuration parameters are used:

-- connStr: The PostgreSQL connection string, i.e. database, user, password,
-- etc.

-- datestyle: The PostgreSQL datestyle to be used

local config = ...

if config.connStr == nil then
	print 'logbook: missing connection string'
	return
end

if trxd.verbose() > 0 then
	print 'initializing the trx-control logbook extension'
end

local function setupDatabase()
	if config.datestyle ~= nil then
		local res <close> = db:exec(string.format(
		    "set datestyle to '%s'", config.datestyle))
	end
end

-- Functions used internally by the logbook extension
local function connectDatabase()
	connStr = config.connStr or
	    'dbname=trx-control fallback_application_name=logbook'

	db = pgsql.connectdb(connStr)

	if db:status() ~= pgsql.CONNECTION_OK then
		print('can not connect to database: ' .. db:errorMessage())
	else
		setupDatabase()
	end
end

local function checkDatabase()
	if db:status() ~= pgsql.CONNECTION_OK then
		connectDatabase()
	end
	return db:status() == pgsql.CONNECTION_OK
end

connectDatabase()

-- Public functions
function logQSO(request)
	if checkDatabase() == false then
		return {
			status = 'Failure',
			reason = 'Database not connected'
		}
	end

	local data = request.data
	if data == nil then
		return {
			status = 'Failure',
			reason = 'Missing request data'
		}
	end

	if data.call == nil or data.operatorCall == nil then
		return {
			status = 'Failure',
			reason = 'Missing mandatory QSO data'
		}
	end

	local res <close> = db:execParams([[
	insert
	  into logbook.logbook (call, name, qso_start, qso_end, qth, locator,
				frequency, mode, operator_call, remarks)
	values ($1, $2, $3::timestamptz, $4::timestamptz, $5, $6, $7::bigint,
		$8, $9, $10)
	]], data.call, data.name, data.qsoStart, data.qsoEnd, data.qth,
	    data.locator, data.frequency, data.mode, data.operatorCall,
	    data.remarks)

	if res:status() == pgsql.PGRES_COMMAND_OK then
		return {
			status = 'Ok',
			message = 'QSO logged'
		}
	else
		return {
			status = 'Failure',
			reason = res:errorMessage()
		}
	end
end

function lookupCallsign(request)
	if checkDatabase() == false then
		return {
			status = 'Failure',
			reason = 'Database not connected'
		}
	end

	local data = request.data
	if data == nil then
		return {
			status = 'Failure',
			reason = 'Missing request data'
		}
	end

	if data.callsign == nil then
		return {
			status = 'Failure',
			reason = 'Missing callsign'
		}
	end

	local res <close> = db:execParams([[
	  select call, name, qso_start as qsoStart, qso_end as qsoEnd, qth,
		 locator, frequency, mode, operator_call as operatorCall,
		 remarks
	    from logbook.logbook
	   where call ilike $1
	order by qso_start
	]], '%' .. data.callsign .. '%')

	return {
		status = 'Ok',
		data = res:copy()
	}
end
