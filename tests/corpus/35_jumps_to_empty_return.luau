-- conditionals deep inside a function that the compiler threads to its final `return` (with no values)
local reports = {}

local function check(kind, leaf, selection)
	if kind then
		if leaf then
			if selection then
				table.insert(reports, "leaf with selection " .. kind)
			end
		elseif not selection then
			table.insert(reports, "missing selection " .. kind)
		end
	end
end

check("A", true, true)
check("B", true, false)
check("C", false, false)
check("D", false, true)
check(nil, true, true)
print(table.concat(reports, "; "))
