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

-- trxctl support functions/command handlers

local function params(data)
end

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
		mTable = getmetatable(value)
		if mTable == nil then
			print(spaces .. linePrefix)
		else
			print(spaces)
			value = mTable
		end
		for tableKey, tableValue in pairs(value) do
			vardump(tableValue, depth, tableKey)
		end
	elseif type(value) == 'function' or type(value) == 'thread'
	    or type(value) == 'userdata' or type(value) == nil then
		print(spaces .. tostring(value))
	else
		print(spaces .. linePrefix .. tostring(value))
	end
end

function call(to, command, param)
	local request = {}

	if command ~= nil then
		request.request = command
	end

	if to ~= nil then
		request.to = to
	end

	if param ~= nil then
		request.data = {}
		for k, v in string.gmatch(param, "(%w+)=(%w+)") do
			request.data[k] = v
		end
	end

	trxctl.writeln(json.encode(request))
	local reply = json.decode(trxctl.readln())

	vardump(reply)
end

local function ts(s)
	s = tostring(s)
	return s:reverse():gsub('(%d%d%d)', '%1.'):reverse():gsub('^[.]', '')
end

function handleStatusUpdate(jsonData)
	local data = json.decode(jsonData)

	if data == nil then
		print ('received unparseable data', data)
	end

	vardump(data)
end

