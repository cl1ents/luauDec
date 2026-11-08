-- `if p(a) and p(b)` with `p` inlined together with its statements: the second test jumps into the else arm
local function isPlainTable(value)
	local ok = false
	if type(value) == "table" then
		ok = getmetatable(value) == nil
	end
	return ok
end

local function deepJoin(target, source)
	local result = table.clone(target)
	for key, value in source do
		if isPlainTable(value) and isPlainTable(result[key]) then
			result[key] = deepJoin(result[key], value)
		else
			result[key] = value
		end
	end
	return result
end

local function describe(t, depth)
	local keys = {}
	for k in t do
		table.insert(keys, tostring(k))
	end
	table.sort(keys)
	local parts = {}
	for _, k in keys do
		local v = t[k]
		table.insert(parts, k .. "=" .. (if type(v) == "table" then "{" .. describe(v, depth + 1) .. "}" else tostring(v)))
	end
	return table.concat(parts, ",")
end

local joined = deepJoin({ a = { x = 1 }, b = 2, c = setmetatable({}, {}) }, { a = { y = 2 }, b = { z = 3 }, c = { w = 4 } })
print(describe(joined, 0))
