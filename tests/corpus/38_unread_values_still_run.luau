-- values whose register is reused before it is read still have to run (O2 inlining with constant conditions)
local log = {}
local M = {}

function M.f()
	table.insert(log, "f")
	return 1
end

local function g(c)
	local x = M.f()
	local y = if c then x else nil
	return y
end

print(g(false), #log)
print(g(true), #log)

-- a value read on one path of an if-expression is computed before it, on every path
local function h(c)
	local x = M.f()
	return if c then x + 1 else 0
end

print(h(false), h(true), #log)
print(table.concat(log, ","))
