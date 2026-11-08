local a, b, c = 7, 3, -2.5

print(a + b, a - b, a * b, a / b, a // b, a % b, a ^ b)
print(-a, -c, - -a, -(a + b), #"hello", "con" .. "cat", a .. b)
print(c + a, c * b - a / 2, (a + b) * (a - b), a + b * c ^ 2)
print(a % -b, -a % b, -a // b, 7.5 // 2, -7.5 % 2, 2 ^ -1, 2 ^ 0.5)
print(10 / 4, 1 / 0 > 1e308, -1 / 0 < -1e308, 0 / 0 ~= 0 / 0)

local function mix(x, y)
    local s = x + y
    local d = x - y
    local p = x * y
    local q = x / (y ~= 0 and y or 1)
    local r = x % (y ~= 0 and y or 1)
    local f = x // (y ~= 0 and y or 1)
    return s, d, p, q, r, f, -s, s ^ 2
end

for i = -3, 3 do
    print(i, mix(i, i - 1))
end

local t = {1, 2, 3, 4}
local u = "abc"
print(#t, #u, #t + #u, -#t, #t * 2 .. "x", #(u .. u))

local x = 1
x = x + 1
x = x * 10 - x / 2
x = -x
x = x % 7
print(x)

local acc = 0
for i = 1, 20 do
    for j = 1, i do
        acc = acc + (i * j) % 7 - (i // j) + (j ^ 2) % 3
    end
end
print(acc)

local s = "n="
for i = 1, 5 do
    s = s .. i .. (i < 5 and "," or "")
end
print(s, #s)

local k1, k2 = 2, 5
print(k1 + 1, 1 + k1, k1 - 1, 1 - k1, k1 * 3, 3 * k1, k1 / 4, 4 / k1, k1 % 3, 3 % k1, k1 ^ 3, 3 ^ k1, k2 // 2, 9 // k1)
print(k1 .. "a", "a" .. k1, k1 .. k2 .. k1, "<" .. -k2 .. ">")
