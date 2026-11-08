-- a `continue` that ends an inner arm: falling off that arm does not reach the loop latch
local out = {}
local i = 0
while i < 6 do
	i += 1
	if i == 1 then
		table.insert(out, "a")
	elseif i == 2 then
		table.insert(out, "b")
		continue
	end
	table.insert(out, i)
end
print(table.concat(out, ","))

-- React's appendAllChildren walk
local function walk(root, found)
	local node = root.child
	while node ~= nil do
		if node.tag == 1 or node.tag == 2 then
			table.insert(found, node.name)
		elseif node.tag == 4 then
		elseif node.child ~= nil then
			node.child.return_ = node
			node = node.child
			continue
		end
		if node == root then
			return
		end
		while node.sibling == nil do
			if node.return_ == nil or node.return_ == root then
				return
			end
			node = node.return_
		end
		node.sibling.return_ = node.return_
		node = node.sibling
	end
end

local leaf1 = {tag = 1, name = "x"}
local leaf2 = {tag = 2, name = "y"}
leaf1.sibling = leaf2
local root = {tag = 3, child = {tag = 3, child = leaf1}}
local found = {}
walk(root, found)
print(table.concat(found, ","))

for j = 1, 4 do
	if j == 1 then
		table.insert(out, "one")
	elseif j % 2 == 0 then
		table.insert(out, "even")
		continue
	end
	table.insert(out, j)
end
print(table.concat(out, ","))
