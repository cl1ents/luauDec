local name = "world"
local n = 42
print(`hello {name}`)
print(`n = {n}, n*2 = {n * 2}, neg = {-n}`)
print(`plain backtick string`)
print(`{name}{name}`)
print(`nested {`inner {name:upper()}`} done`)
print(`braces \{ escaped \} and backtick \``)
print(`tab\tnewline-escape\\n`)
print(`bool {true} nil {nil} float {1.5}`)

local items = {"a", "b", "c"}
for i, v in ipairs(items) do
    print(`item {i}: {v} ({#v})`)
end

local function describe(p)
    return `{p.name} is {p.age} years old{if p.age >= 18 then "" else " (minor)"}`
end
print(describe({name = "Ann", age = 30}))
print(describe({name = "Bo", age = 12}))

local parts = {}
for row = 1, 3 do
    for col = 1, 2 do
        parts[#parts + 1] = `({row},{col})`
    end
end
print(table.concat(parts, " "))

local function fmt(a, b)
    return `{a} + {b} = {a + b}; {a} .. {b} = {a .. b}`
end
print(fmt(1, 2))
print(fmt(10, 20))

local counter = 0
local function nextId()
    counter = counter + 1
    return counter
end
print(`ids {nextId()} {nextId()} {nextId()}`)

local x = 3
local s = `{x}`
local t = `x={x}` .. "!" .. `{x * x}`
print(s, t, #s, type(s))

print(`{string.format("%05.1f", 3.14159)}|{("ab"):rep(2)}|{math.floor(7.9)}`)
