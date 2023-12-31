local curl = require 'curl'

print(curl._DESCRIPTION)
print(curl._VERSION)
print(curl._COPYRIGHT)
print(curl.version())

local info = curl.versionInfo()

for k, v in pairs(info) do
	print(k, v)
end

for k, v in ipairs(info.protocols) do
	print(k, v)
end
