local function compareAll(a, b)
    local r = {}
    if a == b then r[#r + 1] = "==" end
    if a ~= b then r[#r + 1] = "~=" end
    if a < b then r[#r + 1] = "<" end
    if a <= b then r[#r + 1] = "<=" end
    if a > b then r[#r + 1] = ">" end
    if a >= b then r[#r + 1] = ">=" end
    if not (a < b) then r[#r + 1] = "!<" end
    if not (a <= b) then r[#r + 1] = "!<=" end
    if not (a > b) then r[#r + 1] = "!>" end
    if not (a >= b) then r[#r + 1] = "!>=" end
    return table.concat(r, " ")
end
print(compareAll(1, 2))
print(compareAll(2, 2))
print(compareAll(3, 2))
print(compareAll("a", "b"))
print(compareAll("b", "a"))
print(compareAll(0 / 0, 0 / 0))

local function againstConstants(x)
    local r = {}
    if x == 5 then r[#r + 1] = "eq5" end
    if x ~= 5 then r[#r + 1] = "ne5" end
    if x < 5 then r[#r + 1] = "lt5" end
    if x <= 5 then r[#r + 1] = "le5" end
    if x > 5 then r[#r + 1] = "gt5" end
    if x >= 5 then r[#r + 1] = "ge5" end
    if 5 < x then r[#r + 1] = "5lt" end
    if 5 <= x then r[#r + 1] = "5le" end
    if 5 > x then r[#r + 1] = "5gt" end
    if 5 >= x then r[#r + 1] = "5ge" end
    if x == -1.5 then r[#r + 1] = "eqneg" end
    if x > -1.5 then r[#r + 1] = "gtneg" end
    if 1000000 > x then r[#r + 1] = "ltmillion" end
    return table.concat(r, " ")
end
for _, v in ipairs({4, 5, 6, -1.5}) do
    print(v, againstConstants(v))
end

local function constantKinds(v)
    local r = {}
    if v == nil then r[#r + 1] = "isnil" end
    if v ~= nil then r[#r + 1] = "notnil" end
    if v == true then r[#r + 1] = "istrue" end
    if v ~= true then r[#r + 1] = "nottrue" end
    if v == false then r[#r + 1] = "isfalse" end
    if v ~= false then r[#r + 1] = "notfalse" end
    if v == 0 then r[#r + 1] = "iszero" end
    if v ~= 0 then r[#r + 1] = "notzero" end
    if v == "str" then r[#r + 1] = "isstr" end
    if v ~= "str" then r[#r + 1] = "notstr" end
    if nil == v then r[#r + 1] = "nilleft" end
    if "str" == v then r[#r + 1] = "strleft" end
    return table.concat(r, " ")
end
for _, v in ipairs({0, 1, "str", "other", true, false}) do
    print(tostring(v), constantKinds(v))
end
print("nil", constantKinds(nil))

local function stored(a, b)
    local eq = a == b
    local ne = a ~= b
    local lt = a < b
    local le = a <= b
    local gt = a > b
    local ge = a >= b
    local lk = a < 10
    local ek = a == "x"
    local nk = a ~= nil
    return eq, ne, lt, le, gt, ge, lk, ek, nk
end
print(stored(1, 2))
print(stored(5, 5))
print(stored(20, 3))

local function compareInLoops(t, limit)
    local below, equal, above = 0, 0, 0
    for _, v in ipairs(t) do
        if v < limit then
            below = below + 1
        elseif v == limit then
            equal = equal + 1
        else
            above = above + 1
        end
    end
    local i = 1
    while i <= #t and t[i] ~= limit do
        i = i + 1
    end
    return below, equal, above, i
end
print(compareInLoops({1, 5, 3, 7, 5, 9}, 5))
print(compareInLoops({}, 0))

local function mixedTypes()
    local r = {}
    r[#r + 1] = tostring(1 == "1")
    r[#r + 1] = tostring("1" ~= 1)
    r[#r + 1] = tostring({} == {})
    local t = {}
    r[#r + 1] = tostring(t == t)
    r[#r + 1] = tostring(nil == false)
    r[#r + 1] = tostring(math.huge > 1e308)
    r[#r + 1] = tostring(-math.huge < -1e308)
    r[#r + 1] = tostring("abc" < "abd")
    r[#r + 1] = tostring("Z" < "a")
    r[#r + 1] = tostring("" < "a")
    return table.concat(r, " ")
end
print(mixedTypes())

local ok, err = pcall(function()
    return {} < {}
end)
print(ok, type(err))

local function clamp(v, lo, hi)
    if v < lo then
        return lo
    elseif v > hi then
        return hi
    end
    return v
end
print(clamp(-1, 0, 10), clamp(5, 0, 10), clamp(11, 0, 10))
