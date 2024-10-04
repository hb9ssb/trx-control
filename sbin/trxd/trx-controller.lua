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

	local response = {
		status = 'Ok',
		from = name,
		response = request.request
	}

	if request.request == 'set-frequency' then
		response.frequency =
		    driver:setFrequency(tonumber(request.frequency))
	elseif request.request == 'get-frequency' then
		response.frequency, response.mode = driver:getFrequency()
	elseif request.request == 'set-mode' then
		response.band, response.mode =
		    driver:setMode(request.band, request.mode)
	elseif request.request == 'get-mode' then
		response.mode = driver:getMode(request.band)
	elseif request.request == 'get-ptt' then
		response.ptt = driver:getPtt()
	elseif request.request == 'set-ptt' then
		response.ptt = driver:setPtt(request.ptt)
	elseif request.request == 'set-callsign' then
		response.callsign =
		    driver:setCallsign(request.callsign)
	elseif request.request == 'get-callsign' then
		response.callsign = driver:getCallsign()
	elseif request.request == 'set-destination' then
		response.callsign =
		    driver:setDestination(request.callsign)
	elseif request.request == 'get-destination' then
		response.callsign = driver:getDestination()
	elseif request.request == 'get-info' then
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
	elseif request.request == 'lock-trx' then
		driver:setLock()
	elseif request.request == 'unlock-trx' then
		driver:setUnlock()
	else
		response.status = 'Error'
		response.reason = 'Unknown request'
	end

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
