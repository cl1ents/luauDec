-- an inner numeric for at the end of an if branch: O1+ threads its exit jump to the outer loop's latch
local function flatten(arr)
	local result = {}
	local k = 1
	for i = 1, #arr do
		if type(arr[i]) == "table" then
			local sub = flatten(arr[i])
			for j = 1, #sub do
				result[k] = sub[j]
				k += 1
			end
		else
			result[k] = arr[i]
			k += 1
		end
	end
	return result
end

print(table.concat(flatten({ 1, { 2, 3, { 4 } }, 5, {}, { { 6 } } }), ","))

local total = 0
for i = 1, 4 do
	if i % 2 == 0 then
		for j = i, 1, -1 do
			total += j
		end
	end
end
print(total)
