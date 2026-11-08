local function strip(msg)
    if type(msg) == "string" then
        return (msg:gsub("^[^:]*:%d+: ", ""))
    end
    return msg
end

print(pcall(function()
    return 1, 2, 3
end))

print(pcall(function()
    error("plain", 0)
end))

local ok, err = pcall(function()
    error("with location")
end)
print(ok, strip(err))

ok, err = pcall(function()
    error({code = 42})
end)
print(ok, type(err), err.code)

ok, err = pcall(function()
    local t = nil
    return t.field
end)
print(ok, type(err))

ok, err = pcall(error)
print(ok, err)

ok, err = pcall(error, "direct", 0)
print(ok, err)

local function risky(n)
    if n > 3 then
        error("too big: " .. n, 0)
    end
    return n * 2
end
for i = 1, 5 do
    print(i, pcall(risky, i))
end

local function nested(depth)
    if depth == 0 then
        error("bottom", 0)
    end
    local ok2, e = pcall(nested, depth - 1)
    error(e .. "<" .. depth, 0)
end
print(pcall(nested, 3))

local cleanup = {}
local function withCleanup(fail)
    local success, result = pcall(function()
        cleanup[#cleanup + 1] = "start"
        if fail then
            error("failed", 0)
        end
        return "done"
    end)
    cleanup[#cleanup + 1] = success and "ok" or "err"
    return result
end
print(withCleanup(false), withCleanup(true), table.concat(cleanup, ","))

print(select("#", pcall(function() end)))
print(pcall(pcall, error, "x"))

local okArith, errArith = pcall(function()
    return 1 + {}
end)
print(okArith, type(errArith))

local function tryParse(s)
    local n = tonumber(s)
    if n == nil then
        error({input = s}, 0)
    end
    return n
end
local inputs = {"12", "x", "3.5", ""}
for _, s in ipairs(inputs) do
    local good, value = pcall(tryParse, s)
    if good then
        print("parsed", value)
    else
        print("failed", "[" .. value.input .. "]")
    end
end

local errs = 0
for i = 1, 10 do
    local good = pcall(function()
        if i % 3 == 0 then
            error("x", 0)
        end
    end)
    if not good then
        errs += 1
    end
end
print(errs)

local okXp, errXp = xpcall(function()
    error("handled", 0)
end, function(m)
    return "handler: " .. m
end)
print(okXp, errXp)
