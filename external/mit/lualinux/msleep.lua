local linux = require 'linux'

for n = 1, 10 do
	print '.'
	print(linux.msleep(1000))
end

for n = 1, 10 do
	print '.'
	print(linux.msleep(500))
end

for n = 1, 10 do
	print '.'
	print(linux.msleep(100))
end