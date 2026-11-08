local function classify(n)
    if n < 0 then
        return "negative"
    elseif n == 0 then
        return "zero"
    elseif n < 10 then
        return "small"
    elseif n < 100 then
        return "medium"
    else
        return "large"
    end
end
for _, v in ipairs({-5, 0, 3, 42, 1000}) do
    print(v, classify(v))
end

local function grade(score, bonus)
    local result
    if score >= 90 or (bonus and score >= 85) then
        result = "A"
    elseif score >= 80 and not bonus then
        result = "B"
    elseif score >= 80 then
        result = "B+"
    elseif score >= 70 and score < 80 then
        result = "C"
    else
        result = "F"
    end
    return result
end
for _, s in ipairs({95, 87, 82, 75, 10}) do
    print(s, grade(s, false), grade(s, true))
end

local function compound(x, y)
    local r = {}
    if x and y then r[#r + 1] = "and" end
    if x or y then r[#r + 1] = "or" end
    if not x then r[#r + 1] = "notx" end
    if not (x and y) then r[#r + 1] = "nand" end
    if not (x or y) then r[#r + 1] = "nor" end
    if (x or y) and not (x and y) then r[#r + 1] = "xor" end
    if x == y then r[#r + 1] = "eq" end
    if x ~= y then r[#r + 1] = "ne" end
    return table.concat(r, ",")
end
print(compound(true, true))
print(compound(true, false))
print(compound(false, true))
print(compound(false, false))
print(compound(nil, 1))
print(compound(0, ""))

local function noElse(x)
    local v = 0
    if x > 5 then
        v = 1
    end
    if x > 10 then
        v = v + 10
    end
    return v
end
print(noElse(1), noElse(7), noElse(20))

local function emptyBranches(x)
    local hits = 0
    if x == 1 then
    elseif x == 2 then
        hits = 2
    else
    end
    if x then
        hits = hits + 1
    end
    return hits
end
print(emptyBranches(1), emptyBranches(2), emptyBranches(3))

local function fizz(n)
    local out = {}
    for i = 1, n do
        if i % 15 == 0 then
            out[#out + 1] = "FizzBuzz"
        elseif i % 3 == 0 then
            out[#out + 1] = "Fizz"
        elseif i % 5 == 0 then
            out[#out + 1] = "Buzz"
        else
            out[#out + 1] = tostring(i)
        end
    end
    return table.concat(out, " ")
end
print(fizz(20))

local function typeSwitch(v)
    local t = type(v)
    if t == "number" then
        if v ~= v then
            return "nan"
        elseif v == math.floor(v) then
            return "integer"
        else
            return "float"
        end
    elseif t == "string" then
        if #v == 0 then
            return "empty string"
        end
        return "string"
    elseif t == "boolean" then
        return v and "true bool" or "false bool"
    elseif t == "nil" then
        return "nil"
    end
    return "other"
end
for _, v in ipairs({1, 1.5, "", "s", true, false, {}}) do
    print(typeSwitch(v))
end
print(typeSwitch(nil), typeSwitch(0 / 0))

local function chained(a, b, c, d)
    if a and b or c and d then
        return 1
    elseif (a or b) and (c or d) then
        return 2
    elseif a and (b or (c and not d)) then
        return 3
    elseif not a and not b and not c and not d then
        return 4
    end
    return 5
end
local results = {}
for mask = 0, 15 do
    results[#results + 1] = chained(mask % 2 == 1, mask // 2 % 2 == 1, mask // 4 % 2 == 1, mask // 8 % 2 == 1)
end
print(table.concat(results))

local gate = 3
if gate > 1 then
    print("gate open")
    if gate > 2 then
        print("wide open")
    end
elseif gate == 1 then
    print("barely")
end
