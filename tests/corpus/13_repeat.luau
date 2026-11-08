local i = 0
repeat
    i = i + 1
until i >= 5
print(i)

local n = 0
repeat
    local sq = n * n
    n = n + 1
until sq > 50
print(n)

local collected = {}
local k = 10
repeat
    local half = k // 2
    collected[#collected + 1] = half
    k = half
until half <= 1
print(table.concat(collected, ","))

local count = 0
repeat
    count = count + 1
    if count == 3 then
        break
    end
until false
print(count)

local outer = 0
local log = {}
repeat
    outer = outer + 1
    local inner = 0
    repeat
        inner = inner + 1
        local product = outer * inner
        log[#log + 1] = product
    until product >= 6 or inner >= 3
until outer >= 3
print(table.concat(log, " "))

local skipped = {}
local v = 0
repeat
    v = v + 1
    if v % 3 == 0 then
        continue
    end
    skipped[#skipped + 1] = v
until v >= 10
print(table.concat(skipped, " "))

local function digits(num)
    local ds = {}
    repeat
        local d = num % 10
        table.insert(ds, 1, d)
        num = num // 10
    until num == 0
    return table.concat(ds, "-")
end
print(digits(0), digits(7), digits(12345))

local fns = {}
local c = 0
repeat
    c = c + 1
    local snapshot = c
    fns[c] = function()
        return snapshot
    end
until snapshot >= 3
print(fns[1](), fns[2](), fns[3]())

local a, b = 1, 1
repeat
    a, b = b, a + b
until a > 100 and b > 100
print(a, b)

local w = 0
while w < 2 do
    w = w + 1
    local r = 0
    repeat
        r = r + 1
        local done = r >= w * 2
    until done
    print(w, r)
end

local x = 5
repeat
    local y = x - 1
    x = y
until not (y > 2)
print(x)
