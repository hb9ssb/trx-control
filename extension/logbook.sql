-- Copyright (c) 2023, 2024 Marc Balmer HB9SSB
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

-- This must be run by the owner of the logbook database against the logbook
-- database that is being used.

\set version 1

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

insert into logbook.version values(:version) on conflict do nothing;
