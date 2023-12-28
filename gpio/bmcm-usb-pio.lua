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

-- bmcm USB-PIO driver

-- The USB-PIO has three physical ports, keep their status cached.
-- Output direction can only be set by port, not by individual IO pin,
-- port 3 can be set in groups of 4 bits.

local port = {
	{ data = 0x00, direction = 0x00 },
	{ data = 0x00, direction = 0x00 },
	{ data = 0x00, direction = 0x00 }
}

local function getPortData(port)
	gpio.write(string.format('@00P%d?\r', port - 1))
	local res = gpio.read(6)
	return tonumber(string.sub(res, 4, 5), 16)
end

local function setPortData(port, data)
	gpio.write(string.format('@00P%d%2.2X\r', port - 1, data))
	local res = gpio.read(6)
	return tonumber(string.sub(res, 4, 5), 16)
end

local function getPortDirection(port)
	gpio.write(string.format('@00D%d?\r', port - 1))
	local res = gpio.read(6)
	return tonumber(string.sub(res, 4, 5), 16)
end

local function setPortDirection(port, direction)
	gpio.write(string.format('@00D%d%2.2X\r', port - 1, direction))
	local res = gpio.read(4)
end

local function getPort(io)
	return 1 + ((io - 1) // 8)
end

-- direct driver commands

local function initialize()
	print 'Initialize bmcm USB-PIO driver'
	for k, v in pairs(port) do
		v.direction = getPortDirection(k)
		v.data = getPortData(k)
	end
end

local function setOutput(io, value)
	local k = getPort(io)
	local bit = io - ((port - 1) * 8) - 1

	if value == true then
		port[k].data = port[k].data | (1 << bit)
	else
		port[k].data = port[k].data & (port[k].data ~ (1 << bit))
	end
	setPortData(k, port[k].data)
end

local function getInput(io)
	local k = getPort(io)
	local bit = io - ((port - 1) * 8) - 1

	local data = getPortData(k)
	return data & (1 << bit)
end

local function getOutput(io)
	return getInput(io)
end

local function getDirection(io)
	local k = getPort(io)
	local bit = io - ((port - 1) * 8) - 1

	return port[k].direction & (1 << bit)
end

local function setGroupDirection(group, direction)
	local p = 1

	if direction ~= 'in' and direction ~= 'out' then
		return 'not set'
	end

	if group == 1 or group == 2 then
		p = group
		if direction == 'in' then
			port[p].direction = 0xff
		else
			port[p].direction = 0x00
		end
	else
		p = 3

		if group == 3 then
			if direction == 'in' then
				port[p].direction = port[p].direction | 0x0f
			else
				port[p].direction = port[p].direction & 0xf0
			end
		else
			if direction == 'in' then
				port[p].direction = port[p].direction | 0xf0
			else
				port[p].direction = port[p].direction & 0x0f
			end
		end
	end
	setPortDirection(p, port[p].direction)
	return direction
end

local function getGroupDirection(group)
	if group == 1 or group == 2 then
		if port[group].direction == 0x00 then
			return 'in'
		else
			return 'out'
		end
	else
		local d = port[3].direction

		if group == 3 then
			if d & 0x0f then
				return 'out'
			else
				return 'in'
			end
		else
			if d & 0xf0 then
				return 'out'
			else
				return 'in'
			end
		end
	end
end

return {
	name = 'bmcm USB-PIO driver',
	ioNum = 24,

	-- ioGroups are groups of IO pins whose direction can only be set
	-- as a whole group. Set portGroups to nil if the direction of all
	-- IO pins can be individually set.
	-- Note that GPIO pins are 1 based.

	ioGroups = {
		{ from = 1, to = 8 },
		{ from = 9, to = 16 },
		{ from = 17, to = 20 },
		{ from = 21, to = 24 }
	},
	statusUpdatesRequirePolling = true,
	initialize = initialize,
	setOutput = setOutput,
	getInput = getInput,
	getOutput = getOutput,
	setDirection = nil,
	getDirection = getDirection,
	setGroupDirection = setGroupDirection,
	getGroupDirection = getGroupDirection
}
