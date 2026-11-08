-- an if/else at the end of an `if` arm: its `then` arm jumps straight past the outer if/elseif
local function merge(target, ...)
	local index = 1
	for i = 1, select("#", ...) do
		local source = select(i, ...)
		if source then
			for j = 1, #source do
				if type(source[j]) == "table" then
					if target[index] == nil then
						target[index] = "new" .. #source[j]
					else
						target[index] = target[index] .. "+" .. #source[j]
					end
				elseif source[j] ~= false then
					target[index] = tostring(source[j])
				end
				index += 1
			end
		end
	end
	return target
end

print(table.concat(merge({}, { 1, { 2 }, false }, nil, { { 3, 4 } }), ","))
print(table.concat(merge({ "a", "b" }, { { 1 }, { 2, 3 } }), ","))
