local function f()
    return "f"
end
local function g()
    return "g"
end
local function falsy()
    return false
end

local values = {true, false, 0, "", "s"}
for _, a in ipairs(values) do
    for _, b in ipairs(values) do
        print(tostring(a), tostring(b), tostring(a and b), tostring(a or b), tostring(not a), tostring(not not b))
    end
end
print(nil and 1, nil or 1, false or nil, nil or false, 1 and nil)

local function ternary(c)
    return c and f() or g()
end
print(ternary(true), ternary(false), ternary(nil), ternary(0))

local function ifexpr(n)
    local size = if n < 0 then "neg" elseif n == 0 then "zero" elseif n < 10 then "small" else "big"
    local parity = if n % 2 == 0 then "even" else "odd"
    local nested = if n > 0 then (if n > 5 then "gt5" else "le5") else "nonpos"
    return size, parity, nested
end
for _, n in ipairs({-3, 0, 4, 7, 12}) do
    print(n, ifexpr(n))
end

local function defaults(opts)
    opts = opts or {}
    local name = opts.name or "anon"
    local size = opts.size or 10
    local enabled = opts.enabled ~= false
    local label = opts.label and ("<" .. opts.label .. ">") or ""
    return name, size, enabled, label
end
print(defaults())
print(defaults({name = "n", size = 0, enabled = false, label = "L"}))
print(defaults({enabled = nil}))

local function chain(a, b, c)
    local r1 = a and b and c
    local r2 = a or b or c
    local r3 = a and b or c
    local r4 = a or b and c
    local r5 = (a or b) and c
    local r6 = a and (b or c)
    local r7 = not a and b
    local r8 = not (a and b) or c
    return r1, r2, r3, r4, r5, r6, r7, r8
end
print(chain(1, 2, 3))
print(chain(nil, 2, 3))
print(chain(1, false, 3))
print(chain(false, nil, false))

local x = 5
local inRange = x > 0 and x < 10
local outRange = x < 0 or x > 10
local isFive = x == 5
local notFive = not isFive
print(inRange, outRange, isFive, notFive)

local maybe = falsy() or f()
local both = f() and g()
local neither = falsy() and f()
print(maybe, both, neither)

local t = {}
local valueOrCall = t.missing or (function()
    return "lazy"
end)()
print(valueOrCall)

local count = 0
local function sideEffect(v)
    count = count + 1
    return v
end
local _ = sideEffect(false) and sideEffect(true)
local _ = sideEffect(true) or sideEffect(false)
local _ = sideEffect(nil) or sideEffect(1) and sideEffect(2)
print(count)

local m = {}
for i = 1, 6 do
    m[i] = if i % 3 == 0 then "fizz" elseif i % 2 == 0 then i * 10 else (i > 4 and "late" or "early")
end
print(table.concat(m, " "))

local ifcall = (if x > 3 then f else g)()
print(ifcall)

local function minmax(a, b)
    return (a < b and a or b), (a > b and a or b)
end
print(minmax(3, 9), minmax(9, 3), minmax(4, 4))

local str = nil
local len = str and #str or -1
print(len)
str = "abc"
len = str and #str or -1
print(len)

local bool1 = not nil == true
local bool2 = not (1 == 2)
local bool3 = not 1 == 2
print(bool1, bool2, bool3)
