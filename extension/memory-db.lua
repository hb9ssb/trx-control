-- Copyright (c) 2024 - 2026 Marc Balmer HB9SSB
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

local dbVersion = 1

local installationScript = [[
create extension if not exists "uuid-ossp";

create schema if not exists memory;

create table if not exists memory.version (
	version		integer not null
);
create unique index if not exists
	memory_single_row on memory.version((1));

create table if not exists memory.grp (
	id 		uuid primary key default uuid_generate_v4(),
	name		text,
	supplement	text,
	descr		text,
	sort_order	integer
);

create table if not exists memory.mem (
	id		uuid primary key default uuid_generate_v4(),
	name		text,
	supplement	text,
	descr		text,
	type		text,

	rx		numeric,
	tx		numeric,
	shift		numeric,

	mode		text,

	sort_order	integer,

	check		(type in ('quick', 'call',
				'channel', 'repeater'))
);

create table if not exists memory.entry (
	this		uuid references memory.grp(id),
	grp		uuid references memory.grp(id),
	mem		uuid references memory.mem(id),
	sort_order	integer,

	check		(grp is null or mem is null)
);
create index entry_this_idx on memory.entry(this);
create index entry_grp_idx on memory.entry(grp);
create index entry_memory_idx on memory.entry(mem);

insert into memory.version values($1) on conflict do nothing;
]]

local updateStep = {
-- update step from 1 to 2
[[
]],

-- update step from 2 to 3
[[
]]
}

local function installMemoryDatabase(db)
	local res = db:execParams(installationScript, dbVersion)

	if res:status() ~= pgsql.PGRES_COMMAND_OK then
		print('memory: database installadion failed '
		    .. res:errorMessage())
		os.exit(1)
	end
	print 'memory: database successfully installed'
end

local function updateMemoryDatabase(db, currentVersion)
	db:exec('begin')

	for step = currentVersion, dbVersion - 1 do
		print(string.format('memory: update database from version %d '
		    .. 'to version %d ', step, step + 1))
		local res = db:exec(updateStep[step])
		if res:status() ~= pgsql.PGRES_COMMAND_OK then
			print('memory: ' .. res:errorMessage())
			db:exec('rollback')
			return false
		end
	end
	db:exec([[
	update memory.version
	   set version = $1::integer
	]], dbVersion)
	db:exec('commit')
	return true
end

local function getMemoryDatabaseVersion(db)
	local res = db:exec([[
	select version
	  from memory.version
	]])

	if #res == 0 then
		return nil
	else
		return tonumber(res[1].version)
	end
end

local function checkMemoryDatabase(db)
	local version = getMemoryDatabaseVersion(db)

	if version == nil then
		print 'memory: database is not installed, installing it ...'
		installMemoryDatabase(db)
	else
		print(string.format('memory: database version is %d', version))

		if version == dbVersion then
			print 'memory: this is the latest version'
		else
			print('memory: update database to version '
			    .. dbVersion)
			if updateMemoryDatabase(db, version) == true then
				print('memory: database update succeeded')
			else
				print('memory: database update failed')
			end
		end

		db:exec('vacuum analyze')
	end
end

return {
	checkMemoryDatabase = checkMemoryDatabase
}
