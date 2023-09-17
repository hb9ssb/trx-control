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

local function useTrx(trx)
	print('useTrx ' .. trx)
	local d = {
		cmd = 'select-trx',
		name = trx
	}

	trxctl.write(json.encode(d) .. '\n')
	local reply = trxctl.read()
	print(reply)
end

local function listTrx()
	local request = {
		cmd = 'list-trx'
	}

	trxctl.write(json.encode(request) .. '\n')
	local reply = json.decode(trxctl.read())

	for k, v in pairs(reply) do
		print(string.format('%-20s %s on %s', v.name, v.driver,
		    v.device))
	end
end

local function setFrequency(freq)
	local request = {
		cmd = 'set-frequency',
		frequency = freq
	}

	trxctl.write(json.encode(request) .. '\n')
	local reply = trxctl.read()
	print(reply)
end

local function getFrequency(freq)
	local request = {
		cmd = 'get-frequency',
	}

	trxctl.write(json.encode(request) .. '\n')
	local reply = trxctl.read()
	print(reply)
end

return {
	useTrx = useTrx,
	listTrx = listTrx,
	setFrequency = setFrequency,
	getFrequency = getFrequency
}
