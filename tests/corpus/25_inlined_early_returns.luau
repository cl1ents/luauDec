-- roundtrip: xfail O2
-- Functions with early returns from inside loops or nested branches. At -O2 the compiler inlines them, turning each
-- `return` into a jump past the end of the enclosing loops; that has no structured Luau equivalent, so the decompiler
-- marks the jump with a comment instead of reproducing it.

local function loopReturn(t)
    for i, v in ipairs(t) do
        if v == "stop" then
            return i
        end
    end
    return -1
end
print(loopReturn({"a", "stop", "b"}), loopReturn({}))

local function nested(a, b, c)
    local path = ""
    if a then
        path = path .. "a"
        if b then
            path = path .. "b"
            if c then
                path = path .. "c"
            else
                path = path .. "!c"
            end
        else
            path = path .. "!b"
            if c then
                path = path .. "c"
            end
        end
    else
        if b or c then
            path = path .. "bc"
        end
        if not b and not c then
            path = path .. "none"
        end
    end
    return path
end
local bools = {true, false}
for _, a in ipairs(bools) do
    for _, b in ipairs(bools) do
        for _, c in ipairs(bools) do
            print(a, b, c, nested(a, b, c))
        end
    end
end

local function isPrime(n)
    if n < 2 then
        return false
    end
    if n % 2 == 0 then
        return n == 2
    end
    local d = 3
    while d * d <= n do
        if n % d == 0 then
            return false
        end
        d += 2
    end
    return true
end

local primes = {}
for i = 1, 60 do
    if isPrime(i) then
        primes[#primes + 1] = i
    end
end
print(table.concat(primes, " "))

local function whileReturn(n)
    local i = 0
    while true do
        i = i + 1
        if i * 3 >= n then
            return i, i * 3
        end
    end
end
print(whileReturn(1), whileReturn(10), whileReturn(30))

