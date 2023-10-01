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

-- Upper half of trx-control

local driver = {}
local device = ''
local statusUpdateListeners = {}
local numStatusUpdateListeners = 0
local statusUpdates = false
local lastFrequency = 0
local lastMode = ''

local function registerDriver(name, dev, newDriver)
	driver = newDriver
	device = dev

	if type(driver.initialize) == 'function' then
		driver.initialize()
	end
end

local function getTransceiverList()
	return trxd.getTransceiverList()
end

local function setFrequency(freq)
	if type(driver.setFrequency) == 'function' then
		return driver.setFrequency(freq)
	end
end

local function getFrequency()
	return driver.getFrequency()
end

local function setMode(band, mode)
	return driver.setMode(band, mode)
end

local function getMode()
	return driver.getMode()
end

local function lockTransceiver()
	return driver.setLock()
end

local function unlockTransceiver()
	return driver.setUnlock()
end

local function addStatusUpdateListener(fd)
	for k, v in ipairs(statusUpdateListeners) do
		if v == fd then
			return 'already listening'
		end
	end

	statusUpdateListeners[#statusUpdateListeners + 1] = fd
	numStatusUpdateListeners = numStatusUpdateListeners + 1
	return 'listening'
end

local function removeStatusUpdateListener(fd)
	for k, v in ipairs(statusUpdateListeners) do
		if v == fd then
			statusUpdateListeners[k] = nil
			numStatusUpdateListeners = numStatusUpdateListeners - 1
			return 'unlisten'
		end
	end
	return 'not listening'
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

	if request.request == 'list-trx' then
		reply.data = getTransceiverList()
	elseif request.request == 'set-frequency' then
		reply.frequency = setFrequency(tonumber(request.frequency))
	elseif request.request == 'get-frequency' then
		reply.frequency, reply.mode = getFrequency()
	elseif request.request == 'set-mode' then
		reply.band, reply.mode = setMode(request.band, request.mode)
	elseif request.request == 'get-mode' then
		reply.mode = getMode(request.band)
	elseif request.request == 'select-trx' then
		local t = trxd.selectTransceiver(request.name)
		if t ~= nil then
			return string.format('switch-tag:%s',
			    request.name)
		else
			reply = {
				status = 'Error',
				reply = 'select-trx',
				reason = 'No such transceiver',
				name = request.name
			}
		end
	elseif request.request == 'lock-trx' then
		lockTransceiver()
	elseif request.request == 'unlock-trx' then
		unlockTransceiver()
	elseif request.request == 'start-status-updates' then
		addStatusUpdateListener(fd)

		-- if this is the first listener, start the poller or
		-- input handler

		if numStatusUpdateListeners == 1 then
			if driver.statusUpdatesRequirePolling == true then
				trxd.startPolling(device)
			elseif type(driver.startStatusUpdates) == 'function' then
				local eol = driver.startStatusUpdates()
				trxd.startHandling(device, eol)
			else
				reply.status = 'Error'
				reply.reason = 'Can not start status updates'
				removeStatusUpdateListener(fd)
			end
		end

	elseif request.request == 'stop-status-updates' then
		removeStatusUpdateListener(fd)

		-- if this was the last listener, stop the poller or
		-- input handler

		if numStatusUpdateListeners == 0 then
			if driver.statusUpdatesRequirePolling == true then
				trxd.stopPolling(device)
			elseif type(driver.stopStatusUpdates) == 'function' then
				driver.stopStatusUpdates()
				trxd.stopHandling(device)
			else
				reply.status = 'Error'
				reply.reason = 'Can not stop status updates'
			end
		end
	else
		reply.status = 'Error'
		reply.reason = 'Unknown request'
	end

	return json.encode(reply)
end

local function pollHandler(data, fd)
	local frequency, mode = getFrequency()
	if lastFrequency ~= frequency or lastMode ~= mode then
		local status = {
			request = 'status-update',
			status = {
				frequency = frequency,
				mode = mode
			}
		}
		local jsonData = json.encode(status)
		print(jsonData)
		for k, v in pairs(statusUpdateListeners) do
			trxd.sendToClient(v, jsonData)
		end
		lastFrequency = frequency
		lastMode = mode
	else
		return nil
	end
end

-- Handle incoming data from the transceiver
local function dataHandler(data)
	if type(driver.handleStatusUpdates) == 'function' then
		local reply = driver.handleStatusUpdates(data)
		if reply ~= nil then
			local status = {
				request = 'status-update',
				status = reply
			}
			local jsonData = json.encode(status)
			print(jsonData)
			for k, v in pairs(statusUpdateListeners) do
				trxd.sendToClient(v, jsonData)
			end
		end
	end
end

return {
	registerDriver = registerDriver,
	requestHandler = requestHandler,
	pollHandler = pollHandler,
	dataHandler = dataHandler
}
