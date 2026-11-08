local x = 10
x += 5
x -= 3
x *= 2
x /= 4
x //= 1
x %= 4
x ^= 3
print(x)

local s = "a"
s ..= "b"
s ..= s
s ..= 1
print(s)

local up = 1
local function mutate(k)
    up += k
    up *= 2
    up -= 1
    return up
end
print(mutate(1), mutate(2), up)

local t = {n = 0, s = "", list = {1, 2, 3}, nested = {v = 100}}
t.n += 1
t.n *= 10
t.s ..= "x"
t.s ..= t.n
t.list[2] += 40
t.list[#t.list] *= -1
t.nested.v //= 3
t.nested.v %= 7
t["n"] -= 0.5
print(t.n, t.s, table.concat(t.list, ","), t.nested.v)

local key = "dyn"
t[key] = 1
for i = 1, 4 do
    t[key] += i
    t[key .. i] = i
    t[key .. i] ^= 2
end
print(t.dyn, t.dyn1, t.dyn2, t.dyn3, t.dyn4)

local calls = 0
local function idx()
    calls += 1
    return "k"
end
local u = {k = 1}
u[idx()] += 10
u[idx()] ..= "!"
print(u.k, calls)

local function counterFactory()
    local c = 0
    return function(step)
        c += step or 1
        return c
    end, function()
        c //= 2
        return c
    end
end
local add, halve = counterFactory()
add(7)
add()
print(halve(), add(3), halve())

local acc = 0
for i = 1, 10 do
    if i % 2 == 0 then
        acc += i
    else
        acc -= i
    end
    local inner = i
    inner *= inner
    acc += inner % 3
end
print(acc)

local obj = {count = 0}
function obj:tick(n)
    self.count += n
    return self
end
obj:tick(1):tick(2):tick(3)
print(obj.count)

local vals = {1, 2, 3}
for i = 1, #vals do
    vals[i] *= vals[i]
    vals[i] -= 1
end
print(table.concat(vals, " "))

local text = ""
local i = 0
while i < 3 do
    i += 1
    text ..= `[{i}]`
end
print(text)

local depth = 0
local function recurse(n)
    depth += 1
    if n > 0 then
        recurse(n - 1)
    end
end
recurse(5)
print(depth)
