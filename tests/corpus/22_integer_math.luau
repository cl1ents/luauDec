local function sieve(limit)
    local composite = {}
    local count = 0
    for i = 2, limit do
        if not composite[i] then
            count += 1
            for j = i * i, limit, i do
                composite[j] = true
            end
        end
    end
    return count
end
print(sieve(1000))

local function toBase(n, base)
    if n == 0 then
        return "0"
    end
    local digits = "0123456789abcdef"
    local out = ""
    local neg = n < 0
    n = math.abs(n)
    while n > 0 do
        local d = n % base
        out = digits:sub(d + 1, d + 1) .. out
        n = math.floor(n / base)
    end
    return (neg and "-" or "") .. out
end
print(toBase(255, 2), toBase(255, 16), toBase(-42, 8), toBase(0, 10), toBase(123456789, 36 // 2))

local function powmod(b, e, m)
    local result = 1
    b = b % m
    while e > 0 do
        if e % 2 == 1 then
            result = (result * b) % m
        end
        e = math.floor(e / 2)
        b = (b * b) % m
    end
    return result
end
print(powmod(2, 30, 1000007), powmod(3, 200, 13), powmod(7, 0, 5))

local function isqrt(n)
    local x = math.floor(math.sqrt(n))
    while x * x > n do
        x -= 1
    end
    while (x + 1) * (x + 1) <= n do
        x += 1
    end
    return x
end
print(isqrt(0), isqrt(15), isqrt(16), isqrt(1000000), isqrt(999999))

print(math.floor(3.7), math.floor(-3.7), math.ceil(3.2), math.ceil(-3.2), math.abs(-5), math.max(1, 9, 3), math.min(4, -2, 8))
print(math.fmod(7, 3), math.fmod(-7, 3), 7 % 3, -7 % 3, 7 // -2, math.floor(7 / -2))
print(math.clamp(15, 0, 10), math.sign(-3), math.sign(0), math.round(2.5), math.round(-2.5))
print(bit32.band(0xF0, 0x3C), bit32.bor(1, 4), bit32.bxor(5, 3), bit32.lshift(1, 10), bit32.rshift(1024, 3))

local function collatzMax(limit)
    local best, bestLen = 1, 1
    for start = 1, limit do
        local n, len = start, 1
        while n ~= 1 do
            n = if n % 2 == 0 then n // 2 else 3 * n + 1
            len += 1
        end
        if len > bestLen then
            best, bestLen = start, len
        end
    end
    return best, bestLen
end
print(collatzMax(300))

local function digitsSum(n)
    local s = 0
    while n > 0 do
        s += n % 10
        n //= 10
    end
    return s
end
print(digitsSum(0), digitsSum(9), digitsSum(123456789))

local tri = {}
for row = 1, 8 do
    local line = {1}
    local prev = tri[row - 1]
    if prev then
        for k = 2, row - 1 do
            line[k] = prev[k - 1] + prev[k]
        end
        line[row] = 1
    end
    tri[row] = line
end
print(table.concat(tri[8], " "))

local h = 5381
for i = 1, #"round trip" do
    h = (h * 33 + string.byte("round trip", i)) % 4294967296
end
print(h)
