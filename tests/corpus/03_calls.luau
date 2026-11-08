local function none() end
local function one() return 1 end
local function three() return 1, 2, 3 end
local function count(...) return select("#", ...) end
local function pack(...) return {n = select("#", ...), ...} end

print(count(), count(nil), count(nil, nil), count(three()), count(three(), 10), count((three())))
print(three())
print((three()))
print(three(), three())
print(none(), one(), three())
print(count(none()), count((none())), count(none(), none()))

local function forward(...)
    return ...
end
print(forward(1, nil, 3))
print(count(forward(nil, nil)))
print(select(2, "a", "b", "c"), select(-1, "a", "b", "c"))
print(select("#", forward()))

local function varsum(...)
    local s = 0
    for i = 1, select("#", ...) do
        local v = select(i, ...)
        if v then
            s = s + v
        end
    end
    return s
end
print(varsum(1, 2, 3), varsum(), varsum(three()), varsum(5, three()), varsum(three(), 5))

local function head(first, ...)
    return first, select("#", ...)
end
print(head(), head(1), head(1, 2, 3), head(three()))

local function apply(f, ...)
    return f(...)
end
print(apply(math.max, 3, 9, 4), apply(varsum, three()), apply(forward, "x", "y"))

local function args3(a, b, c)
    return tostring(a) .. "|" .. tostring(b) .. "|" .. tostring(c)
end
print(args3(1), args3(1, 2), args3(1, 2, 3, 4), args3(three()), args3(0, three()), args3((three())))

local p = pack(1, nil, 3, nil)
print(p.n, p[1], p[2], p[3], p[4])

local function nested(...)
    local inner = function(...)
        return select("#", ...), ...
    end
    return inner(inner(...))
end
print(nested(), nested("a"), nested("a", "b"))

local function chain(n, ...)
    if n == 0 then
        return ...
    end
    return chain(n - 1, n, ...)
end
print(chain(5))
print(count(chain(3, "z")))

local t = {forward(1, 2), forward(3, 4)}
print(#t, t[1], t[2], t[3])

local a, b, c, d = three()
print(a, b, c, d)
local e, f = (three())
print(e, f)
local g, h, i = 0, three()
print(g, h, i)

print(string.format("%d-%d-%d", three()))
print(math.max(three()), math.min(5, three()))
print(tostring(nil), type((none())), type(one()))
