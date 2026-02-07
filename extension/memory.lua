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

-- The memory system for trx-control.

local log = require 'linux.sys.log'
local pgsql = require 'pgsql'
local memorydb = require 'memory-db'

-- The configuration is stored in trxd.yaml under the extension.  The following
-- configuration parameters are used:

-- connStr: The PostgreSQL connection string, i.e. database, user, password,
-- etc.

-- datestyle: The PostgreSQL datestyle to be used

local config = ...

local connStr = config.connStr or 'dbname=trx-control'

if trxd.verbose() > 0 then
	log.syslog('notice', 'initializing the trx-control memory extension')
end

local function setupDatabase()
	if config.datestyle ~= nil then
		db:exec(string.format("set datestyle to '%s'",
		    db:escapeString(config.datestyle)))
	end
end

-- Functions used internally by the logbook extension
local function connectDatabase()
	connStr = config.connStr or
	    'dbname=trx-control fallback_application_name=logbook'

	db = pgsql.connectdb(connStr)

	if db:status() ~= pgsql.CONNECTION_OK then
		log.syslog('err', 'memory: can not connect to database: '
		    .. db:errorMessage())
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
memorydb.checkMemoryDatabase(db)

-- Public functions

function getToplevel(request)
	if checkDatabase() == false then
		return {
			status = 'Failure',
			response = 'getToplevel',
			reason = 'Database not connected'
		}
	end

	local entries = {}

	local res = db:exec([[
	  select 'group' as entry, id, name, supplement, descr
	    from memory.grp
	   where id not in (select grp from memory.entry)
	order by sort_order, name, supplement
	]])

	if res:status() ~= pgsql.PGRES_TUPLES_OK then
		return {
			status = 'Failure',
			response = 'getToplevel',
			reason = res:errorMessage()
		}
	end
	for tuple in res:tuples() do
		entries[#entries + 1] = tuple:copy()
	end

	local res = db:exec([[
	  select 'memory' as entry, id, name, supplement, descr, type,
		 tx, rx, shift, mode
	    from memory.mem
	   where id not in (select mem from memory.entry)
	order by sort_order, name, supplement
	]])

	if res:status() ~= pgsql.PGRES_TUPLES_OK then
		return {
			status = 'Failure',
			response = 'getToplevel',
			reason = res:errorMessage()
		}
	end
	for tuple in res:tuples() do
		entries[#entries + 1] = tuple:copy()
	end

	return {
			status = 'Ok',
			response = 'getToplevel',
			entries = entries
	}
end

function getMemoryGroup(request)
	if checkDatabase() == false then
		return {
			status = 'Failure',
			response = 'getMemoryGroup',
			reason = 'Database not connected'
		}
	end

	if request.group == nil then
		return {
			status = 'Failure',
			response = 'getMemoryGroup',
			reason 'No memory group specified'
		}
	end

	local res = db:execParams([[
	  select grp, mem, b.name as grp_name,
		 b.supplement as grp_supplement, b.descr as grp_descr,
		 m.name as mem_name, m.supplement as mem_supplement,
		 m.descr as mem_descr, type, rx, tx, shift, mode
	    from memory.entry e
	    join memory.mem m on e.mem = m.id and m.id is not null
	    join memory.grp b on e.grp = b.id and b.id is not null
	   where bank = $1::uuid
	order by sort_order
	]], request.group)

	if res:status() ~= pgsql.PGRES_TUPLES_OK then
		return {
			status = 'Failure',
			response = 'getMemoryGroup',
			reason = res:errorMessage()
		}
	end

	local entries = {
	}

	for tuple in res:tuples() do
		if tuple.bank == '' then
			local entry = {
				entry = 'group',
				id = tuple.grp,
				name = tuple.grp_name,
				supplement = tuple.grp_supplement,
				descr = tuple.grp_descr
			}
			entries[#entries + 1] = entry
		else
			local entry = {
				entry = 'memory',
				id = tuple.mem,
				name = tuple.mem_name,
				supplement = tuple.mem_supplement,
				descr = tuple.mem_descr,
				type = tuple.type,
				rx = tuple.rx,
				tx = tuple.tx,
				shift = tuple.shift,
				mode = tuple.mode

			}
			entries[#entries + 1] = entry
		end
	end

	return {
			status = 'Ok',
			response = 'getMemoryGroup',
			entries = entries
	}
end

function addMemoryGroup(request)
	if checkDatabase() == false then
		return {
			status = 'Failure',
			response = 'addMemoryGroup',
			reason = 'Database not connected'
		}
	end

	if request.name == nil or request.name == '' then
		return {
			status = 'Failure',
			response = 'addMemoryGroup',
			reason = 'Missing memory group name'
		}
	end

	local name = request.name
	local supplement = request.supplement
	local descr = request.descr

	if supplement ~= nil and supplement == '' then
		supplement = nil
	end

	if descr ~= nil and descr == '' then
		descr = nil
	end

	local res = db:execParams([[
	   insert
	     into memory.grp (name, supplement, descr)
	   values ($1, $2, $3)
	returning id
	]], name, supplement, descr)

	if res:status() ~= pgsql.PGRES_TUPLES_OK then
		return {
			status = 'Failure',
			response = 'addMemoryGroup',
			reason = res:errorMessage()
		}
	end

	return {
		status = 'Ok',
		response = 'addMemoryGroup',
		id = res[1].id
	}
end

function addMemory(request)
	if checkDatabase() == false then
		return {
			status = 'Failure',
			response = 'addMemory',
			reason = 'Database not connected'
		}
	end

	if request.name == nil or request.name == '' then
		return {
			status = 'Failure',
			response = 'addMemory',
			reason = 'Missing memory group name'
		}
	end

	if request.rx == nil or request.rx == '' or request.rx == json.null() then
		return {
			status = 'Failure',
			response = 'addMemory',
			reason = 'Missing frequency'
		}
	end

	local name = request.name
	local supplement = request.supplement
	local descr = request.descr
	local rx = request.rx

	if supplement ~= nil and supplement == '' then
		supplement = nil
	end

	if descr ~= nil and descr == '' then
		descr = nil
	end

	if descr ~= nil and descr == '' then
		descr = nil
	end

	local res = db:execParams([[
	   insert
	     into memory.mem (name, supplement, descr, rx)
	   values ($1, $2, $3, $4::numeric)
	returning id
	]], name, supplement, descr, rx)

	if res:status() ~= pgsql.PGRES_TUPLES_OK then
		return {
			status = 'Failure',
			response = 'addMemory',
			reason = res:errorMessage()
		}
	end

	return {
		status = 'Ok',
		response = 'addMemory',
		id = res[1].id
	}
end
