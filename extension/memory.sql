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

-- This must be run by the owner of the trx-control database against the
-- trx-control database that is being used.

\set version 1

create extension if not exists "uuid-ossp";

drop schema if exists memory cascade;

create schema if not exists memory;

create table if not exists memory.version (
	version		integer not null
);
create unique index if not exists memory_single_row on memory.version((1));

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

	check		(type in ('quick', 'call', 'channel', 'repeater'))
);

-- Memories and memory banks are keep in the same table and not in
-- separate tables so they can be arbitrarily sorted.  If they were in
-- two separate table (memory members and bank members), sorting would
-- become a lot more difficult
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

insert into memory.version values(:version) on conflict do nothing;
