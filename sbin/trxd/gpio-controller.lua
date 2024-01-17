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

-- Upper half of gpio-control

local driver = {}
local device = ''

local statusUpdates = false

local function registerDriver(name, dev, newDriver)
	driver = newDriver
	device = dev

	if type(driver.initialize) == 'function' then
		driver.initialize()
	end
end

-- Handle request from a network client
local function requestHandler(data, fd)
	local request = json.decode(data)

	if request == nil then
		return json.encode({
			status = 'Error',
			reason = 'Invalid input data or no input data at all'
		})
	end

	if request.request == nil or #request.request == 0 then
		return json.encode({
			status = 'Error',
			reason = 'No request'
		})
	end

	local reply = {
		status = 'Ok',
		reply = request.request
	}

	if request.request == 'get-info' then
		reply.name = driver.name or 'unspecified'
		reply.ioNum = driver.ioNum or 'unspecified'
		if driver.ioGroups ~= nil then
			reply.ioGroups = driver.ioGroups
		end
	elseif request.request == 'set-output' then
		print(request.io, request.value)
		if request.value == 'true' then
			driver.setOutput(request.io, true)
		else
			driver.setOutput(request.io, false)
		end
	elseif request.request == 'get-input' then
	elseif request.request == 'get-output' then
	elseif request.request == 'set-direction' then
		if driver.setDirection == nil then
			reply.status = 'Error'
			reply.reason =
			    'IO direction can not be programmed individually'
		else
		end
	elseif request.request == 'get-direction' then
		if driver.setDirection == nil then
			reply.status = 'Error'
			reply.reason =
			    'IO direction can not be programmed individually'
		else
		end
	elseif request.request == 'set-group-direction' then
		if driver.setGroupDirection == nil then
			reply.status = 'Error'
			reply.reason = 'IO groups not supported by driver'
		else
			if request.group == nil or request.direction == nil then
				reply.status = 'Error'
				reply.reason = 'No group or direction specified'
			else
				reply.status = 'Ok'
				reply.direction = driver.setGroupDirection(
				    tonumber(request.group),
				    request.direction)
			end
		end
	elseif request.request == 'get-group-direction' then
		if driver.setGroupDirection == nil then
			reply.status = 'Error'
			reply.reason = 'IO groups not supported by driver'
		else
			if request.group == nil  then
				reply.status = 'Error'
				reply.reason = 'No group specified'
			else
				reply.status = 'Ok'
				reply.direction = driver.getGroupDirection(
				    tonumber(request.group))
			end
		end
	else
		reply.status = 'Error'
		reply.reason = 'Unknown request'
	end

	return json.encode(reply)
end

local function pollHandler(data, fd)
	--[[
	local frequency, mode = driver:getFrequency()
	if lastFrequency ~= frequency or lastMode ~= mode then
		local status = {
			request = 'status-update',
			status = {
				frequency = frequency,
				mode = mode
			}
		}

		local jsonData = json.encode(status)
		gpioController.notifyListeners(jsonData)
		lastFrequency = frequency
		lastMode = mode
	else
		return nil
	end
	--]]
	return nil
end

return {
	registerDriver = registerDriver,
	requestHandler = requestHandler,
	pollHandler = pollHandler
}
