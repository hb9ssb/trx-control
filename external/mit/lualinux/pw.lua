local linux = require 'linux'
local pwd = require 'linux.pwd'

io.write('username: ')
local user = io.read()

local t = pwd.getpwnam(user)
local s = pwd.getspnam(user)
local salt = string.match(s.sp_pwdp, '(%$[^%$]+%$[^%$]+%$)')

local pw = linux.getpass('password: ')

cpw = linux.crypt(pw, salt)

if cpw == s.sp_pwdp then
	print('match')
else
	print('no match')
end

