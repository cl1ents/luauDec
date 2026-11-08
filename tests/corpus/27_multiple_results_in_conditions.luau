-- several results of one call, read by a conditional expression
local ok, value = pcall(function()
	return 5
end)
local picked = if ok then value else nil
print(picked)

local ok2, err = pcall(error, "boom")
print(if ok2 then "fine" else tostring(err):match("boom"))

local a, b = string.find("hello world", "wor")
print(a and b or -1)
