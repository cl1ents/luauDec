local i = 0
while i < 5 do
    i = i + 1
end
print(i)

local n = 27
local steps = 0
while n ~= 1 do
    if n % 2 == 0 then
        n = n // 2
    else
        n = 3 * n + 1
    end
    steps = steps + 1
end
print(steps)

local found = nil
local x = 0
while true do
    x = x + 1
    if x * x > 200 then
        found = x
        break
    end
end
print(found)

local evens = {}
local k = 0
while k < 20 do
    k = k + 1
    if k % 2 == 1 then
        continue
    end
    evens[#evens + 1] = k
end
print(table.concat(evens, ","))

local pairsFound = {}
local a = 1
while a <= 5 do
    local b = a
    while b <= 5 do
        if (a + b) % 3 == 0 then
            pairsFound[#pairsFound + 1] = a .. "+" .. b
        end
        if b == 4 then
            break
        end
        b = b + 1
    end
    a = a + 1
end
print(table.concat(pairsFound, " "))

local function gcd(p, q)
    while q ~= 0 do
        p, q = q, p % q
    end
    return p
end
print(gcd(48, 18), gcd(17, 5), gcd(100, 75))

local stack = {1, 2, 3, 4, 5}
local popped = {}
while #stack > 0 do
    popped[#popped + 1] = table.remove(stack)
end
print(table.concat(popped, ""))

local outer = 0
local total = 0
while outer < 4 do
    outer = outer + 1
    local inner = 0
    while inner < outer do
        inner = inner + 1
        if inner == 2 then
            continue
        end
        local w = 0
        while w < inner do
            w = w + 1
            total = total + outer * inner * w
        end
    end
end
print(total)

local fns = {}
local j = 0
while j < 3 do
    j = j + 1
    local captured = j
    fns[j] = function()
        return captured * 100
    end
end
print(fns[1](), fns[2](), fns[3]())

local s = "a,b,,c"
local parts = {}
local pos = 1
while pos <= #s + 1 do
    local comma = string.find(s, ",", pos, true)
    if not comma then
        parts[#parts + 1] = "[" .. s:sub(pos) .. "]"
        break
    end
    parts[#parts + 1] = "[" .. s:sub(pos, comma - 1) .. "]"
    pos = comma + 1
end
print(table.concat(parts))

local cond = true
local iterations = 0
while cond and iterations < 10 do
    iterations = iterations + 1
    cond = iterations % 7 ~= 0
end
print(iterations, cond)

local m = 10
while m > 0 and not (m == 3) do
    m = m - 1
end
print(m)

local guard = 0
while guard < 3 do
    guard = guard + 1
    if guard == 1 then
        continue
    elseif guard == 2 then
        print("second")
    else
        print("third")
    end
end
