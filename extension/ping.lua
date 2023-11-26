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

-- The trx-control ping extension

local config = ... or {}

print 'initializing the trx-control ping extension'

local function vardump(value, depth, key)
	local linePrefix = ""
	local spaces = ""

	if key ~= nil then
		linePrefix = "[" .. key .. "] = "
	end

	if depth == nil then
		depth = 0
	else
		depth = depth + 1
		for i = 1, depth do spaces = spaces .. " " end
	end

	if type(value) == 'table' then
		local mTable = getmetatable(value)
		if mTable == nil then
			print(spaces .. linePrefix .. "(table) ")
		else
			print(spaces .. "(metatable) ")
			value = mTable
		end
		for tableKey, tableValue in pairs(value) do
			vardump(tableValue, depth, tableKey)
		end
	elseif type(value) == 'function' or type(value) == 'thread'
	    or type(value) == 'userdata' or type(value) == nil then
		print(spaces .. tostring(value))
	else
		print(spaces .. linePrefix .. "(" .. type(value) .. ") " ..
		    tostring(value))
	end
end

function ping(request)
	vardump(request)
	return { status = 'Ok', reply = 'pong' }
end
