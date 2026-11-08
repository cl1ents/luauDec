local out = {}
for i = 1, 5 do
    out[#out + 1] = i
end
print(table.concat(out, " "))

out = {}
for i = 10, 1, -3 do
    out[#out + 1] = i
end
print(table.concat(out, " "))

out = {}
for i = 0, 1, 0.25 do
    out[#out + 1] = i
end
print(table.concat(out, " "))

out = {}
for i = 1, -1, -0.5 do
    out[#out + 1] = i
end
print(table.concat(out, " "))

for i = 5, 1 do
    print("never", i)
end

local n = 4
local total = 0
for i = 1, n do
    for j = i, n do
        for k = j, n, 2 do
            total = total + i * 100 + j * 10 + k
        end
    end
end
print(total)

local start, stop, step = 3, 15, 4
out = {}
for i = start, stop, step do
    out[#out + 1] = i
end
print(table.concat(out, " "))

local function range(a, b, s)
    local r = {}
    for i = a, b, s or 1 do
        r[#r + 1] = i
    end
    return #r, r[1], r[#r]
end
print(range(1, 10))
print(range(10, 1, -1))
print(range(1, 2, 0.1))
print(range(-5, 5, 5))

local captured = {}
for i = 1, 3 do
    captured[i] = function()
        return i
    end
    i = i * 10
    out[i] = i
end
print(captured[1](), captured[2](), captured[3]())

local lastSeen
for i = 1, 100 do
    if i % 17 == 0 then
        lastSeen = i
        break
    end
end
print(lastSeen)

local table2 = {}
for row = 1, 3 do
    local line = {}
    for col = row, 1, -1 do
        line[#line + 1] = row .. ":" .. col
    end
    table2[row] = table.concat(line, ",")
end
print(table.concat(table2, " | "))

local count = 0
for i = 1, 10 do
    if i % 2 == 0 then
        continue
    end
    for j = 1, 10 do
        if j > i then
            break
        end
        count = count + 1
    end
end
print(count)

local sum = 0
for i = 1, 3 do
    local inner = 0
    for j = 1, i * 2 do
        local add = function()
            inner = inner + j
            sum = sum + i
        end
        add()
    end
    print(i, inner, sum)
end

for i = 2^52 - 2, 2^52 do
    print(i)
end

for i = #"abc", 0, -1 do
    print(i, ("abc"):sub(i, i))
end

local t = {5, 4, 3, 2, 1}
for i = #t, 1, -1 do
    print(i, t[i])
end
