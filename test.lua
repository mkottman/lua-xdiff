require 'xdiff'

s1 = [[
this here
is
sample
file content
]]

print(xdiff.diff(s1, assert(io.open('test.lua'))))
