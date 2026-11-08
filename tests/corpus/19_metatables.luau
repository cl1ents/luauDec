local defaults = {color = "red", size = 1}
local obj = setmetatable({size = 5}, {__index = defaults})
print(obj.color, obj.size, rawget(obj, "color"))

local computed = setmetatable({}, {
    __index = function(t, k)
        local v = k * 2
        rawset(t, k, v)
        return v
    end,
})
print(computed[1], computed[21], rawget(computed, 21), rawget(computed, 3))

local Point = {}
Point.__index = Point
Point.__add = function(a, b)
    return setmetatable({x = a.x + b.x, y = a.y + b.y}, Point)
end
Point.__sub = function(a, b)
    return setmetatable({x = a.x - b.x, y = a.y - b.y}, Point)
end
Point.__mul = function(a, k)
    if type(a) == "number" then
        a, k = k, a
    end
    return setmetatable({x = a.x * k, y = a.y * k}, Point)
end
Point.__eq = function(a, b)
    return a.x == b.x and a.y == b.y
end
Point.__lt = function(a, b)
    return a.x < b.x or (a.x == b.x and a.y < b.y)
end
Point.__le = function(a, b)
    return not (b < a)
end
Point.__unm = function(a)
    return setmetatable({x = -a.x, y = -a.y}, Point)
end
Point.__tostring = function(p)
    return "P(" .. p.x .. "," .. p.y .. ")"
end
Point.__len = function(p)
    return math.abs(p.x) + math.abs(p.y)
end
Point.__concat = function(a, b)
    return tostring(a) .. tostring(b)
end

local function P(x, y)
    return setmetatable({x = x, y = y}, Point)
end

local p1, p2 = P(1, 2), P(3, 4)
print(tostring(p1 + p2), tostring(p2 - p1), tostring(p1 * 3), tostring(2 * p2), tostring(-p1))
print(p1 == P(1, 2), p1 ~= p2, p1 < p2, p2 <= p1, p1 <= P(1, 2), #P(-3, 4))
print(p1 .. p2, p1 .. "!", tostring(p1 + p2 * 2 - p1))

local Callable = setmetatable({calls = 0}, {
    __call = function(self, a, b)
        self.calls += 1
        return a + b, self.calls
    end,
})
print(Callable(1, 2))
print(Callable(10, 20))

local log = {}
local proxy = setmetatable({}, {
    __index = function(_, k)
        log[#log + 1] = "get " .. k
        return k .. "?"
    end,
    __newindex = function(t, k, v)
        log[#log + 1] = "set " .. k
        rawset(t, k, v)
    end,
})
print(proxy.a)
proxy.b = 1
proxy.b = 2
print(proxy.b, proxy.c)
print(table.concat(log, ", "))

local chainBase = {base = "base"}
local chainMid = setmetatable({mid = "mid"}, {__index = chainBase})
local chainTop = setmetatable({top = "top"}, {__index = chainMid})
print(chainTop.top, chainTop.mid, chainTop.base, chainTop.none)

local Stack = {}
Stack.__index = Stack
function Stack.new()
    return setmetatable({n = 0}, Stack)
end
function Stack:push(v)
    self.n += 1
    self[self.n] = v
end
function Stack:pop()
    local v = self[self.n]
    self[self.n] = nil
    self.n -= 1
    return v
end
local st = Stack.new()
for i = 1, 5 do
    st:push(i * i)
end
print(st:pop(), st:pop(), st.n, getmetatable(st) == Stack)

local readonly = setmetatable({}, {
    __index = {k = 1},
    __newindex = function()
        error("readonly", 0)
    end,
})
print(pcall(function()
    readonly.k = 2
end))
print(readonly.k)
