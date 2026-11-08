counter = 0
config = {name = "global", level = 1}

function increment(by)
    counter = counter + (by or 1)
    return counter
end

function config.describe()
    return config.name .. "@" .. config.level
end

function config:bump()
    self.level = self.level + 1
end

increment()
increment(5)
print(counter, increment(10))

config:bump()
config.bump(config)
print(config.describe())

globalList = {}
for i = 1, 3 do
    globalList[i] = i * i
end
print(table.concat(globalList, ","))

local function useGlobal()
    return counter * 2, undefinedGlobal, type(undefinedGlobal)
end
print(useGlobal())

undefinedGlobal = "now defined"
print(useGlobal())
undefinedGlobal = nil
print(undefinedGlobal)

function makeGlobalCounter()
    local count = 0
    return function()
        count = count + 1
        counter = counter + count
        return count, counter
    end
end
local gc = makeGlobalCounter()
gc()
print(gc())

a, b, c = 1, 2
print(a, b, c)
a, b = b, a
print(a, b)

local localShadow = counter
counter = "shadowed"
print(localShadow, counter)

flag = false
if not flag then
    flag = true
end
print(flag)

total = 0
for _, v in ipairs({1, 2, 3, 4}) do
    total += v
end
print(total)

nested = {deep = {deeper = {}}}
nested.deep.deeper.value = 7
nested.deep.deeper.value *= 6
print(nested.deep.deeper.value)

function recursiveGlobal(n)
    if n <= 0 then
        return 0
    end
    return n + recursiveGlobal(n - 1)
end
print(recursiveGlobal(10))

print(type(math), type(string.format), type(print), math.pi > 3, string.rep("-", 3))
