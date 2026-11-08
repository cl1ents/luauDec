local function nothing()
    return
end

local function single(x)
    return x
end

local function multiple(x)
    return x, x * 2, x * 3
end

local function early(x)
    if x < 0 then
        return "negative"
    elseif x == 0 then
        return "zero", 0
    end
    for i = 1, 10 do
        if i * i > x then
            return "root-ish", i
        end
    end
    return "big", x, nil
end

local function tail(n, acc)
    if n == 0 then
        return acc
    end
    return tail(n - 1, acc + n)
end

local function passVarargs(...)
    return ...
end

local function prefixVarargs(...)
    return "first", ...
end

local function callResult(x)
    return multiple(x)
end

local function truncated(x)
    return (multiple(x))
end

local function mixed(x)
    return x, multiple(x)
end

local function conditional(flag, ...)
    if flag then
        return ...
    else
        return select("#", ...)
    end
end

print(select("#", nothing()), nothing())
print(single(5), single(nil), single("s"))
print(multiple(2))
for _, v in ipairs({-5, 0, 3, 50, 200}) do
    print(early(v))
end
print(select("#", early(200)))
print(tail(100, 0))
print(passVarargs(1, 2, nil, 4))
print(prefixVarargs(), prefixVarargs(1, 2))
print(callResult(4))
print(truncated(4))
print(mixed(4))
print(conditional(true, "a", "b"), conditional(false, "a", "b", "c"))

local function nestedReturns(a, b)
    if a then
        if b then
            return "ab"
        end
        return "a"
    else
        if b then
            return "b"
        else
            return "none"
        end
    end
end
print(nestedReturns(true, true), nestedReturns(true, false), nestedReturns(false, true), nestedReturns(false, false))

local function returnsFunction(k)
    return function(...)
        return k, ...
    end
end
print(returnsFunction(1)(2, 3))

local function returnTable(...)
    return {...}, select("#", ...)
end
local t, n = returnTable(9, 8, 7)
print(#t, n, t[1], t[3])

local function exprReturn(a, b)
    return a + b, a - b, a .. b, a == b, a < b, not a
end
print(exprReturn(3, 4))
