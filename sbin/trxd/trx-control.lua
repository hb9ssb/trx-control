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
local frequencyListeners = {}

local function registerDriver(newDriver)
	driver = newDriver

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

local function setMode(mode)
	return driver.setMode(mode)
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

	local reply = {
		status = 'Ok',
		reply = request.request
	}

	if request.request == 'list-trx' then
		reply.data = getTransceiverList()
	elseif request.request == 'set-frequency' then
		reply.frequency = setFrequency(request.frequency)
	elseif request.request == 'get-frequency' then
		reply.frequency = getFrequency()
	elseif request.request == 'set-mode' then
		reply.mode = setMode(request.mode, request.band)
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
	end
	return json.encode(reply)
end

-- Handle incoming data from the transceiver
local function dataHandler(data)
end

-- Inihibit talking to the real hardware for now
trx.write = function (data)
	print(string.format('write %s to the trx', data))
end

trx.read = function () return '' end

return {
	registerDriver = registerDriver,
	requestHandler = requestHandler,
	dataHandler = dataHandler
}
