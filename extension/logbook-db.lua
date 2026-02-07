-- Copyright (c) 2025 - 2026 Marc Balmer HB9SSB
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

local pgsql = require 'pgsql'

local dbVersion = 2

local installationScript = [[
create schema if not exists logbook;

create table if not exists logbook.version (
	version		integer not null
);
create unique index if not exists logbook_single_row on logbook.version((1));

create table if not exists logbook.logbook (
	call		text not null,
	name		text not null,
	qth		text,
	locator		text,
	operator_call	text not null,

	qso_start	timestamptz,
	qso_end		timestamptz,
	frequency	bigint,
	mode		text,
	remarks		text
);
create index logbook_call_idx on logbook.logbook(call);
]]

local updateStep = {
-- update step from 1 to 2
[[
alter table logbook.logbook
	add column report_given text,
	add column report_received text,
	add column serial text;
]],

-- update step from 2 to 3
[[
]]
}

local function installLogbookDatabase(db)
	local res = db:execParams(installationScript, dbVersion)

	if res:status() ~= pgsql.PGRES_COMMAND_OK then
		print('logbook: database installadion failed '
		    .. res:errorMessage())
		os.exit(1)
	end
	print 'logbook: database successfully installed'
end

local function updateLogbookDatabase(db, currentVersion)
	db:exec('begin')

	for step = currentVersion, dbVersion - 1 do
		print(string.format('logbook: update database from version %d '
		    .. 'to version %d ', step, step + 1))
		local res = db:exec(updateStep[step])
		if res:status() ~= pgsql.PGRES_COMMAND_OK then
			print('logbook: ' .. res:errorMessage())
			db:exec('rollback')
			return false
		end
	end
	local res = db:exec([[
	update logbook.version
	   set version = $1::integer
	]], dbVersion)

	local res = db:exec('commit')
	return true
end

local function getLogbookDatabaseVersion(db)
	local res = db:exec([[
	select version
	  from logbook.version
	]])

	if #res == 0 then
		return nil
	else
		return tonumber(res[1].version)
	end
end

local function checkLogbookDatabase(db)
	local version = getLogbookDatabaseVersion(db)

	if version == nil then
		print 'logbook: database is not installed, installing it ...'
		installLogbookDatabase(db)
	else
		print(string.format('logbook: database version is %d', version))

		if version == dbVersion then
			print 'logbook: this is the latest version'
		else
			print('logbook: update database to version '
			    .. dbVersion)
			if updateLogbookDatabase(db, version) == true then
				print('logbook: database update succeeded')
			else
				print('logbook: database update failed')
			end
		end

		db:exec('vacuum analyze')
	end
end

return {
	checkLogbookDatabase = checkLogbookDatabase
}
