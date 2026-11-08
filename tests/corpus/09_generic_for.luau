local function sortedPairs(t)
    local keys = {}
    for k in pairs(t) do
        table.insert(keys, k)
    end
    table.sort(keys, function(a, b)
        return tostring(a) < tostring(b)
    end)
    local i = 0
    return function()
        i = i + 1
        local k = keys[i]
        if k ~= nil then
            return k, t[k]
        end
        return nil
    end
end

local record = {b = 2, a = 1, d = 4, c = 3}
for k, v in sortedPairs(record) do
    print(k, v)
end

local sum = 0
local count = 0
for k, v in pairs(record) do
    sum = sum + v
    count = count + 1
end
print(sum, count)

local list = {"x", "y", "z"}
for i, v in ipairs(list) do
    print(i, v)
end

for i, v in next, list do
    print("next", i, v)
end

for i in ipairs(list) do
    print("index only", i)
end

for _, v in list do
    print("generalized", v)
end

local gsum = 0
for k, v in {10, 20, 30, x = 5} do
    gsum = gsum + v + (type(k) == "number" and k or 0)
end
print(gsum)

local function range(n)
    local i = 0
    return function()
        i = i + 1
        if i <= n then
            return i, i * i, i * i * i
        end
    end
end

for a, b, c in range(4) do
    print(a, b, c)
end

local function statelessIter(limit, current)
    if current < limit then
        return current + 1, (current + 1) * 2
    end
end
for i, d in statelessIter, 3, 0 do
    print("stateless", i, d)
end

local function words(s)
    return string.gmatch(s, "%a+")
end
local collected = {}
for w in words("the quick brown fox") do
    collected[#collected + 1] = w:upper()
end
print(table.concat(collected, "-"))

local nested = {{1, 2}, {3, 4, 5}, {}, {6}}
local flat = {}
for i, row in ipairs(nested) do
    for j, v in ipairs(row) do
        flat[#flat + 1] = i .. "." .. j .. "=" .. v
    end
end
print(table.concat(flat, " "))

local matrix = {a = {x = 1, y = 2}, b = {x = 3, y = 4}}
for name, inner in sortedPairs(matrix) do
    for axis, value in sortedPairs(inner) do
        print(name, axis, value)
    end
end

local found
for i, v in ipairs({5, 8, 13, 21}) do
    if v % 2 == 0 then
        found = i
        break
    end
end
print(found)

local odd = {}
for i, v in ipairs({1, 2, 3, 4, 5, 6}) do
    if v % 2 == 0 then
        continue
    end
    odd[#odd + 1] = v
end
print(table.concat(odd, ","))

local closures = {}
for i, v in ipairs({"p", "q", "r"}) do
    closures[i] = function()
        return v .. i
    end
end
print(closures[1](), closures[2](), closures[3]())

local function multi(t)
    return function(state, idx)
        idx = idx + 1
        local v = state[idx]
        if v then
            return idx, v, #v, v:sub(1, 1), v:upper()
        end
    end, t, 0
end
for i, v, len, first, up in multi({"ab", "cde", "f"}) do
    print(i, v, len, first, up)
end

local stops = 0
for i, v in ipairs({1, 2, nil, 4}) do
    stops = stops + v
end
print(stops)

local deep = 0
for _, a in ipairs({1, 2}) do
    for _, b in ipairs({10, 20}) do
        for _, c in ipairs({100, 200}) do
            deep = deep + a * b * c
        end
    end
end
print(deep)

local keys = {}
for k in pairs({alpha = true, beta = true, gamma = true}) do
    keys[#keys + 1] = k
end
table.sort(keys)
print(table.concat(keys, ","))
