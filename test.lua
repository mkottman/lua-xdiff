require 'xdiff'

str = [[
this here
is
sample
file content
1
2
3
4
5
6
7
8
9
10
]]

str = str:sub(1, 33) .. 'something not in source' .. str:sub(39)

local src = assert(io.open('test.lua')):read('*a')

-- simple interface - use string sources, string output
local diff = xdiff.diff(str, src)
print(diff)


-- advanced interface - string/file source, write to function
local added, removed = 0,0
xdiff.diff(str, io.open('test.lua'), function(s)
	if s:sub(1,2) == '@@' then
		-- new hunk: -oldstart,oldlen +newstart,newlen
		if added + removed ~= 0 then
			print('Hunk', 'added: '..added, 'removed: '..removed)
		end
		added, removed = 0,0
	elseif #s == 1 then
		-- indicator: ' ' = no change, '-' = line removed, '+' = line added
		if s == '+' then added = added + 1
		elseif s == '-' then removed = removed + 1
		end
	else
		-- 1 line of content
	end
end)
print('Hunk', 'added: '..added, 'removed: '..removed)


-- patch str + diff -> src
local patched = xdiff.patch(str, diff)
assert(patched == src, 'patched result differs')

-- patch src - diff -> str
local revpatched = xdiff.patch(src, diff, { reverse = true })
assert(revpatched == str, 'reverse patch results differ')

print('OK')
