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

-- Upper half of trx-control

local name = ''
local driver = {}
local device = ''

local statusUpdates = false
local lastFrequency = 0
local lastMode = ''

local function registerDriver(destination, dev, newDriver)
	name = destination
	driver = newDriver
	device = dev

	if type(driver.initialize) == 'function' then
		driver:initialize()
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
		from = name,
		reply = request.request
	}

	if request.request == 'set-frequency' then
		reply.frequency = driver:setFrequency(tonumber(request.frequency))
	elseif request.request == 'get-frequency' then
		reply.frequency, reply.mode = driver:getFrequency()
	elseif request.request == 'set-mode' then
		reply.band, reply.mode = driver:setMode(request.band, request.mode)
	elseif request.request == 'get-mode' then
		reply.mode = driver:getMode(request.band)
	elseif request.request == 'get-info' then
		reply.name = driver.name or 'unspecified'
		reply.frequencyRange = driver.frequencyRange or {
			min = 0,
			max = 0
		}
		if driver.validModes ~= nil then
			reply.operatingModes = {}
			for k, v in pairs(driver.validModes) do
				reply.operatingModes[#reply.operatingModes + 1]
				    = k
			end
		end
		if driver.capabilities ~= nil then
			reply.capabilities = driver.capabilities
		end
	elseif request.request == 'lock-trx' then
		driver:setLock()
	elseif request.request == 'unlock-trx' then
		driver:serUnlock()
	else
		reply.status = 'Error'
		reply.reason = 'Unknown request'
	end

	return json.encode(reply)
end

local function pollHandler(data, fd)
	local frequency, mode = driver:getFrequency()
	if lastFrequency ~= frequency or lastMode ~= mode then
		local status = {
			request = 'status-update',
			from = name,
			status = {
				frequency = frequency,
				mode = mode
			}
		}

		local jsonData = json.encode(status)
		trxController.notifyListeners(jsonData)
		lastFrequency = frequency
		lastMode = mode
	else
		return nil
	end
end

-- Handle incoming data from the transceiver
local function dataHandler(data)
	if type(driver.handleStatusUpdates) == 'function' then
		local reply = driver:handleStatusUpdates(data)
		if reply ~= nil then
			local status = {
				request = 'status-update',
				from = name,
				status = reply
			}
			local jsonData = json.encode(status)
			trxController.notifyListeners(jsonData)
		end
	end
end

return {
	registerDriver = registerDriver,
	requestHandler = requestHandler,
	pollHandler = pollHandler,
	dataHandler = dataHandler
}
