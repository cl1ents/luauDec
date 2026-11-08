local function fact(n)
    if n <= 1 then
        return 1
    end
    return n * fact(n - 1)
end
print(fact(1), fact(5), fact(10))

local function makeCounter(start, step)
    local value = start
    local function inc()
        value = value + step
        return value
    end
    local function get()
        return value
    end
    local function reset()
        value = start
    end
    return inc, get, reset
end

local inc, get, reset = makeCounter(10, 3)
inc()
inc()
print(get(), inc(), get())
reset()
print(get(), inc())

local inc2, get2 = makeCounter(0, -1)
inc2()
print(get(), get2())

local fns = {}
for i = 1, 5 do
    fns[i] = function()
        return i * i
    end
end
local results = {}
for i = 1, #fns do
    results[i] = fns[i]()
end
print(table.concat(results, " "))

local shared = {}
local j = 0
while j < 4 do
    j = j + 1
    local captured = j * 10
    shared[j] = function(delta)
        captured = captured + delta
        return captured
    end
end
print(shared[1](1), shared[1](1), shared[2](5), shared[4](0), j)

local byRef = 1
local function readRef()
    return byRef
end
local function writeRef(v)
    byRef = v
end
writeRef(42)
print(readRef(), byRef)
byRef = byRef + 1
print(readRef())

local constant = 7
local function readConst()
    return constant * 2
end
print(readConst())

local function adder(x)
    return function(y)
        return function(z)
            return x + y + z
        end
    end
end
print(adder(1)(2)(3), adder(10)(20)(30))

local function outer()
    local level1 = 1
    local function middle()
        local level2 = 2
        local function inner()
            level1 = level1 + 100
            level2 = level2 + 10
            return level1 + level2
        end
        local r1 = inner()
        local r2 = inner()
        return r1, r2, level2
    end
    local a, b, c = middle()
    return a, b, c, level1
end
print(outer())

local function isEven(n)
    if n == 0 then
        return true
    end
    local function isOdd(m)
        if m == 0 then
            return false
        end
        return isEven(m - 1)
    end
    return isOdd(n - 1)
end
print(isEven(10), isEven(7))

local memo = {}
local function fib(n)
    if n < 2 then
        return n
    end
    local cached = memo[n]
    if cached then
        return cached
    end
    local v = fib(n - 1) + fib(n - 2)
    memo[n] = v
    return v
end
print(fib(30), fib(50))

local grid = {}
for row = 1, 3 do
    for col = 1, 3 do
        local key = row * 10 + col
        grid[#grid + 1] = function()
            return key + row - col
        end
    end
end
local out = {}
for idx, f in ipairs(grid) do
    out[idx] = f()
end
print(table.concat(out, ","))

local loopVar
for k = 1, 3 do
    local capture = function()
        return k
    end
    loopVar = capture
end
print(loopVar())

local acc = {}
for _, word in ipairs({"a", "bb", "ccc"}) do
    acc[#acc + 1] = function(suffix)
        return word .. suffix .. #word
    end
end
print(acc[1]("!"), acc[2]("?"), acc[3]("."))

local function compose(...)
    local fs = {...}
    return function(x)
        for i = #fs, 1, -1 do
            x = fs[i](x)
        end
        return x
    end
end
local double = function(x) return x * 2 end
local square = function(x) return x * x end
print(compose(double, square)(3), compose(square, double)(3), compose()(5))
