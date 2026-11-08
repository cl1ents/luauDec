local a, b, c = 1, 2
print(a, b, c)

a, b = b, a
print(a, b)

local x, y, z = 1, 2, 3
x, y, z = z, x, y
print(x, y, z)

local p, q = 10
p, q = q, p
print(p, q)

local t = {1, 2, 3}
t[1], t[3] = t[3], t[1]
print(t[1], t[2], t[3])

local i = 1
i, t[i] = i + 1, "changed"
print(i, t[1], t[2])

local function two()
    return "first", "second"
end
local m, n, o = two()
print(m, n, o)
m, n, o = 0, two()
print(m, n, o)
m, n = (two())
print(m, n)

local s1, s2, s3, s4 = "a", "b", "c", "d"
s1, s2, s3, s4 = s4, s3, s2, s1
print(s1 .. s2 .. s3 .. s4)

local function fib(k)
    local prev, cur = 0, 1
    for _ = 1, k do
        prev, cur = cur, prev + cur
    end
    return prev
end
print(fib(10), fib(40))

local u
print(u)
u = 5
local v = u
u = u + 1
print(u, v)

do
    local shadow = "outer"
    do
        local shadow = "inner"
        print(shadow)
    end
    print(shadow)
    local shadow = shadow .. "-redeclared"
    print(shadow)
end

local w = 1
local w = w + 1
local w = w * 10
print(w)

local obj = {pos = {x = 1, y = 2}}
obj.pos.x, obj.pos.y = obj.pos.y, obj.pos.x
print(obj.pos.x, obj.pos.y)

local arr = {5, 3, 8, 1}
for pass = 1, #arr do
    for k = 1, #arr - pass do
        if arr[k] > arr[k + 1] then
            arr[k], arr[k + 1] = arr[k + 1], arr[k]
        end
    end
end
print(table.concat(arr, " "))

local g1, g2 = nil, nil
print(g1, g2)
local h1, h2, h3 = nil
print(h1, h2, h3)
local r1, r2 = true, false
r1, r2 = r2, r1
print(r1, r2)

local count = 0
local function bump()
    count = count + 1
    return count
end
local e1, e2, e3 = bump(), bump(), bump()
print(e1, e2, e3, count)

local unused1, unused2 = bump(), bump()
print(count)

local q1, q2 = 1, 2
local q3 = q1 + q2
q1 = q3
q2 = q1 * q3
q3 = q2 - q1
print(q1, q2, q3)
