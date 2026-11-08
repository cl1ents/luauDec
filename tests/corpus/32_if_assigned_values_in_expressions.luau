-- a value assigned by an `if` at the start of a loop body, then read twice by an if-expression
local function scaled(attributes, scale, overrides)
	local out = {}
	for _, attribute in attributes do
		local override = if overrides then overrides[attribute.name] else nil
		local value = if override ~= nil then override else attribute.value
		table.insert(out, attribute.name .. "=" .. tostring(value * scale))
	end
	return table.concat(out, " ")
end

local attributes = { { name = "a", value = 1 }, { name = "b", value = 2 } }
print(scaled(attributes, 10, nil))
print(scaled(attributes, 10, { b = 5 }))
