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
local frequencyListeners = {}
local lastFrequency = 0
local lastMode = ''

local function registerDriver(name, dev, newDriver)
	print(string.format('register driver for %s on %s', name, dev))

	driver = newDriver
	device = dev

	if driver.statusUpdatesRequirePolling == true then
		print('this transceiver requires polling for status updates')
	else
		print('this transceiver supports automatic status updates')
	end
	if type(driver.initialize) == 'function' then
		driver.initialize()
	end
end

local function getTransceiverList()
	return trxd.getTransceiverList()
end

local function setFrequency(freq)
	return driver.setFrequency(freq)
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

local function listenFrequency(fd)
	print 'listen for frequency changes'
	frequencyListeners[#frequencyListeners + 1] = fd
	return 'listening'
end

local function unlistenFrequency(fd)
	print 'unlisten for frequency changes'

	for k, v in ipairs(frequencyListeners) do
		if v == fd then
			frequencyListeners[k] = nil
			return 'unlisten'
		end
	end
	return 'not listening'
end

-- Handle request from a network client
local function requestHandler(data)
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
	elseif request.request == 'start-status-updates' then
		if driver.statusUpdatesRequirePolling == true then
			trxd.startPolling(device)
		elseif type(driver.startStatusUpdates) == 'function' then
			local eol = driver.startStatusUpdates()
			trxd.startHandling(device, eol)
		else
			reply.status = 'Error'
			reply.reason = 'Can not start status updates'
		end
	elseif request.request == 'stop-status-updates' then
		if driver.statusUpdatesRequirePolling == true then
			trxd.stopPolling(device)
		elseif type(driver.stopStatusUpdates) == 'function' then
			driver.stopStatusUpdates()
			trxd.stopHandling(device)
		else
			reply.status = 'Error'
			reply.reason = 'Can not stop status updates'
		end
	elseif request.request == 'status-update' then
		local frequency, mode = getFrequency()
		if lastFrequency ~= frequency or lastMode ~= mode then
			reply.frequency = frequency
			reply.mode = mode
			lastFrequency = frequency
			lastMode = mode
		else
			return nil
		end
	else
		reply.status = 'Error'
		reply.reason = 'Unknown request'
	end
	return json.encode(reply)
end

-- Handle incoming data from the transceiver
local function dataHandler(data)
end

return {
	registerDriver = registerDriver,
	requestHandler = requestHandler,
	dataHandler = dataHandler
}
