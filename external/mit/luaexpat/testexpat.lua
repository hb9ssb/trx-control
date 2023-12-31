local expat = require 'expat'

local function vardump(value, depth, key)
	local linePrefix = ""
	local spaces = ""

	if key ~= nil then
		linePrefix = "[" .. key .. "] = "
	end

	if depth == nil then
		depth = 0
	else
		depth = depth + 1
		for i = 1, depth do spaces = spaces .. " " end
	end

	if type(value) == 'table' then
		mTable = getmetatable(value)
		if mTable == nil then
			print(spaces .. linePrefix)
		else
			print(spaces)
			-- value = mTable
		end
		for tableKey, tableValue in pairs(value) do
			vardump(tableValue, depth, tableKey)
		end
	elseif type(value) == 'function' or type(value) == 'thread'
	    or type(value) == 'userdata' or type(value) == nil then
		print(spaces .. tostring(value))
	else
		print(spaces .. linePrefix .. tostring(value))
	end
end

local data = {
	address = {
		xmlattr = {
			type = 'private',
			gender = 'male'
		},
		name = {
			xmltext = 'Marc'
		},
		surname = {
			xmlattr = {
				spelling = 'French',
				region = 'Switzerland'
			},
			xmltext = 'Balmer'
		},

	}
}

local compactData = {
	address = {
		xmlattr = {
			type = 'private',
			gender = 'male'
		},
		name = 'Marc',
		country = 'Switzerland',
		surname = {
			xmlattr = {
				spelling = 'French',
				region = 'Switzerland'
			},
			xmltext = 'Balmer'
		},
		flag = true,
		noflag = false,
		singular = {
			xmlattr = {
				color = 'red'
			}
		}
	}
}


print 'dump of data before encoding:\n'
vardump(data)

local encoded = expat.encode(data, 'utf-8')

print '\nxmls encoded data:\n'
print(encoded)

print '\ndump of decoded xml data:\n'

local decoded = expat.decode(encoded)
vardump(decoded)

encoded = expat.encode(compactData, 'utf-8')
print '\nxml encoded compact data:\n'
print(encoded)

print '\ndump of decoded compact xml data:\n'

decoded = expat.decode(encoded)
vardump(decoded)

