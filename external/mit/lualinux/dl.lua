local dl = require 'linux.dl'

local libm <close>, error = dl.open('/usr/lib64/libm.so.6', 'lazy')
print(libm, error)

local f, error = libm.floor
print(f, error)
print(dl.sym(libm, 'floor'))
