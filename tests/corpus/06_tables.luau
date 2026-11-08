local function sortedKeys(t)
    local keys = {}
    for k in pairs(t) do
        keys[#keys + 1] = k
    end
    table.sort(keys, function(a, b)
        local ta, tb = type(a), type(b)
        if ta ~= tb then
            return ta < tb
        end
        return a < b
    end)
    return keys
end

local function dump(t, indent)
    indent = indent or ""
    for _, k in ipairs(sortedKeys(t)) do
        local v = t[k]
        if type(v) == "table" then
            print(indent .. tostring(k) .. ":")
            dump(v, indent .. "  ")
        else
            print(indent .. tostring(k) .. " = " .. tostring(v))
        end
    end
end

local function three()
    return 1, 2, 3
end

local empty = {}
local array = {10, 20, 30, 40}
local record = {name = "rec", size = 3, ["key with space"] = true, [5] = "five"}
local mixed = {1, 2, x = "x", 3, y = {nested = {deep = "yes"}}, [10] = 100, [-1] = "neg", [1.5] = "float"}
local multret = {three()}
local multretTail = {0, three()}
local multretMiddle = {three(), 0}
local truncated = {(three())}

print(#empty, next(empty))
print(#array, array[1], array[4], array[5])
print(#multret, #multretTail, #multretMiddle, #truncated)
dump(record)
dump(mixed)

local function varargsTable(...)
    local t = {...}
    local u = {n = select("#", ...), ...}
    local v = {"head", ...}
    return #t, u.n, #v, v[#v]
end
print(varargsTable(4, 5, 6))
print(varargsTable())

local big = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
    51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, 62, 63, 64, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
    91, 92, 93, 94, 95, 96, 97, 98, 99, 100,
    101, 102, 103, 104, 105, 106, 107, 108, 109, 110, three(),
}
local total = 0
for _, v in ipairs(big) do
    total = total + v
end
print(#big, total, big[50], big[51], big[111], big[113])

local a, b = 7, "s"
local bigDynamic = {
    a, b, a + 1, b .. b, a, b, a, b, a, b, a, b, a, b, a, b, a, b, a, b,
    a, b, a, b, a, b, a, b, a, b, a, b, a, b, a, b, a, b, a, b,
    a, b, a, b, a, b, a, b, a, b, a, b, a, b, a, b, a, b, a, b,
    "end",
}
print(#bigDynamic, bigDynamic[3], bigDynamic[4], bigDynamic[60], bigDynamic[61])

local t = {}
t[1] = "one"
t.two = 2
t["three"] = 3
local key = "dyn"
t[key] = "dynamic"
t[key .. "2"] = "dynamic2"
t[true] = "bool"
t.two += 1
t.three *= 10
t[key] ..= "!"
t.counter = 0
for i = 1, 5 do
    t.counter += i
end
dump(t)

local nested = {inner = {list = {1, 2, 3}, count = 0}}
nested.inner.count += 1
nested.inner.list[2] = nested.inner.list[2] * 100
nested.inner.list[#nested.inner.list + 1] = 4
nested["inner"]["extra"] = {}
nested.inner.extra.value = "deep"
dump(nested)

local matrix = {}
for i = 1, 4 do
    matrix[i] = {}
    for j = 1, 4 do
        matrix[i][j] = i * j
    end
end
local rows = {}
for i = 1, 4 do
    rows[i] = table.concat(matrix[i], " ")
end
print(table.concat(rows, " / "))

local list = {}
table.insert(list, "b")
table.insert(list, 1, "a")
table.insert(list, "c")
print(table.concat(list, ","), table.remove(list), table.concat(list, ","))
print(table.unpack({1, 2, 3}))
print(select("#", table.unpack({1, nil, 3}, 1, 3)))

local constTable = {x = 1, y = 2, z = 3}
local function readXYZ(p)
    return p.x + p.y * 10 + p.z * 100
end
print(readXYZ(constTable), readXYZ({x = 4, y = 5, z = 6}))

local keys = {"alpha", "beta", "gamma"}
local index = {}
for i, k in ipairs(keys) do
    index[k] = i
    index[i] = k
end
print(index.alpha, index.gamma, index[2], #index)

local tableOfFuncs = {
    add = function(x, y) return x + y end,
    sub = function(x, y) return x - y end,
    [1] = function() return "first" end,
}
print(tableOfFuncs.add(2, 3), tableOfFuncs.sub(2, 3), tableOfFuncs[1]())
