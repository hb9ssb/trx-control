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
local functions = {}

local device = ''

local statusUpdates = false
local lastFrequency = 0
local lastMode = ''

local function registerDriver(destination, dev, newDriver)
	name = destination
	driver = newDriver
	device = dev

	functions = {
		['set-frequency'] = type(driver.setFrequency) == 'function'
		    and driver.setFrequency or nil,
		['get-frequency'] = type(driver.getFrequency) == 'function'
		    and driver.getFrequency or nil,
		['set-mode'] = type(driver.setMode) == 'function'
		    and driver.setMode or nil,
		['get-mode'] = type(driver.getMode) == 'function'
		    and driver.getMode or nil,
		['set-ptt'] = type(driver.setPtt) == 'function'
		    and driver.setPtt or nil,
		['get-ptt'] = type(driver.getPtt) == 'function'
		    and driver.getPtt or nil,
		['set-callsign'] = type(driver.setCallsign) == 'function'
		    and driver.setCallsign or nil,
		['get-callsign'] = type(driver.getCallsign) == 'function'
		    and driver.getCallsign or nil,
		['set-destination'] = type(driver.setDestination) == 'function'
		    and driver.setDestination or nil,
		['get-destination'] = type(driver.getDestination) == 'function'
		    and driver.getDestination or nil,
		['get-info'] = getInfo,
		['lock-trx'] = type(driver.setLock) == 'function'
		    and driver.setLock or nil,
		['unlock-trx'] = type(driver.setUnlock) == 'function'
		    and driver.setUnlock or nil,
	}

	if type(driver.initialize) == 'function' then
		driver:initialize()
	end
end

local function getInfo(driver, request, response)
	response.name = driver.name or 'unspecified'
	response.frequencyRange = driver.frequencyRange or {
		min = 0,
		max = 0
	}
	if driver.validModes ~= nil then
		response.operatingModes = {}
		for k, v in pairs(driver.validModes) do
			response.operatingModes[#response.operatingModes + 1]
			    = k
		end
	end
	if driver.capabilities ~= nil then
		response.capabilities = driver.capabilities
	end
	response.audio = driver.audio
end

local function notImplemented(response)
	response.status = 'Failure'
	response.reason = 'Function unknown or not implemented'
	return json.encode(response)
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

	local response = {
		status = 'Ok',
		from = name,
		response = request.request
	}

	local handler = functions[request.request]

	if handler == nil then
		return notImplemented(response)
	end

	handler(driver, request, response)
	return json.encode(response)
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
		local response = driver:handleStatusUpdates(data)
		if response ~= nil then
			local status = {
				request = 'status-update',
				from = name,
				status = response
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
