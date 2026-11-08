local s = "Hello, World"
print(#s, s:len(), s:upper(), s:lower(), s:reverse())
print(s:sub(1, 5), s:sub(-5), s:sub(8, -1), s:sub(100), s:sub(0))
print(s:find("World"), s:find("o", 6), s:find("xyz"), s:find(".", 1, true))
print(s:match("(%a+), (%a+)"))
print(s:gsub("o", "0"))
print(s:gsub("%a+", function(w)
    return w:lower()
end))
print(("x"):rep(3), ("ab"):rep(2, "-"), ("z"):rep(0))
print(s:byte(1), s:byte(-1), string.char(72, 105))
print(string.format("%5d|%-5d|%05.2f|%x|%X|%s|%q", 42, 42, 3.14159, 255, 255, "str", "q\"t"))
print(string.format("%g %g %g", 1e20, 0.1, 100))

local words = {}
for w in ("one two  three"):gmatch("%S+") do
    words[#words + 1] = w
end
print(#words, table.concat(words, "|"))

local function split(str, sep)
    local out = {}
    local start = 1
    while true do
        local i, j = str:find(sep, start, true)
        if not i then
            out[#out + 1] = str:sub(start)
            break
        end
        out[#out + 1] = str:sub(start, i - 1)
        start = j + 1
    end
    return out
end
print(table.concat(split("a,b,,c", ","), "/"), #split("", ","))

local function trim(str)
    return (str:gsub("^%s+", ""):gsub("%s+$", ""))
end
print("[" .. trim("   padded   ") .. "]")

local function caesar(str, shift)
    return (str:gsub("%a", function(c)
        local base = c:match("%l") and 97 or 65
        return string.char((c:byte() - base + shift) % 26 + base)
    end))
end
print(caesar("Hello, World", 3), caesar(caesar("Round Trip", 7), 19))

local count = select(2, ("banana"):gsub("a", "a"))
print(count)

local parts = {}
for k, v in ("a=1, b=2, c=3"):gmatch("(%w+)=(%w+)") do
    parts[#parts + 1] = v .. k
end
print(table.concat(parts, " "))

local built = {}
for i = 1, 5 do
    built[i] = string.rep(tostring(i), i)
end
print(table.concat(built, "."))

print(("%d items"):format(3), ("CamelCase"):lower():upper(), ("  x  "):len())
print(tostring(12):len(), (tostring(1.5) .. "0"):rep(2))
print(string.split and #string.split("a b c", " ") or 3)
print(("abc"):split("")[2] or "none")
print(("test"):upper():lower():sub(2, 3):rep(2))
