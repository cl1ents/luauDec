-- a conditional inside an `if` arm that jumps straight to the code after the whole if/else
local NIL = {}
local cache = {}
local created = 0

local function lookup(class, prop)
	local classCache = cache[class]
	if classCache then
		local value = classCache[prop]
		if value == NIL then
			return true, nil
		end
		if value ~= nil then
			return true, value
		end
	else
		classCache = {}
		cache[class] = classCache
	end
	created += 1
	local ok = prop ~= "bad"
	local default = if ok and prop ~= "none" then class .. "." .. prop else nil
	if ok then
		if default == nil then
			classCache[prop] = NIL
		else
			classCache[prop] = default
		end
	end
	return ok, default
end

print(lookup("Part", "Size"))
print(lookup("Part", "Size"))
print(lookup("Part", "none"))
print(lookup("Part", "none"))
print(lookup("Part", "bad"))
print(lookup("Model", "Name"))
print(created)
