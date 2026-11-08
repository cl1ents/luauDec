-- a local declared before the closures that use it, then defined as a recursive function
local M = {}
local countdown

function M.run(x)
	return countdown(x, 3)
end

function countdown(x, n)
	if n > 0 then
		return countdown(x + 1, n - 1)
	end
	return x
end

print(M.run(1))
print(M.run(10))

-- same, with the shared variable reassigned later
local step
local function apply(v)
	return step(v)
end
function step(v)
	if v > 100 then
		return v
	end
	return step(v * 2)
end
print(apply(3))
step = function(v)
	return -v
end
print(apply(3))
