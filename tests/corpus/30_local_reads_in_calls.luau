-- a function in a local is called with arguments loaded before it (the O2 inliner does this)
local function compare(a, b, f)
	if a and b then
		return f(a[1], b[1]) + f(a[2], b[2]) * 10
	end
	return a or b
end
local function minMax(min1, max1, min2, max2)
	return compare(min1, min2, math.min), compare(max1, max2, math.max)
end
print(minMax({ 1, 2 }, { 5, 6 }, { 3, 0 }, { 4, 9 }))
print(minMax(nil, { 5, 6 }, 7, { 4, 9 }))

-- locals that a call in the same expression assigns keep their read order
local x = 1
local function bump()
	x += 1
	return x
end
print(x, bump(), x)

local y = 10
local function add(a, b)
	return a + b
end
local g = add
print(g(y, (function()
	y = 20
	return 1
end)()), y)
