print(0, -0, 1, -1, 3.25, -3.25, 1e10, -1e-10, 1e300, -1e300, 2^53, 0.1, 1/3)
print(1e308 * 10, -1e308 * 10, math.huge, -math.huge, 0x7fffffff, 0xff, 1_000_000, 0b1011)
print(true, false, nil, not nil, not true, not 0)
print("plain", 'single', "with \"double\" quotes", 'with \'single\' quotes', "mixed ' and \"")
print("tab\there", "nl\\n", "bell\a?", "\65\066\x43", "\u{48}\u{49}", "back\\slash", "nul[\0]" == "nul[\0]")
print(#"nul[\0]", ("\0\1\2"):byte(1, -1))
print([[long
string]], [==[with ]] inside]==])
print("unicode: \u{263A}", #"\u{263A}")
print("\r\n" == "\13\10", "\z
       continued")

local big = 9007199254740993
local tiny = 5e-324
local negzero = -0.0
print(big, tiny > 0, 1 / negzero, negzero == 0)

local values = {0, -1, 1.5, -2.75, 123456789, 1e15, 1e16, 123.456e2, 2^-20}
for i, v in ipairs(values) do
    print(i, v, -v, v * 2, tostring(v))
end

local strings = {"", " ", "a\tb", "quote\"", "apos'", "\\", "\n", "]]", "[["}
for i, s in ipairs(strings) do
    print(i, #s, s == strings[i], string.format("%q", s))
end

local function constants()
    return nil, true, false, 42, -42, "str", 0.5
end
print(constants())

local n = nil
local f = false
local tr = true
print(n, f, tr, n == nil, f == false, tr == true)
