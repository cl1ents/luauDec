local Account = {}
Account.__index = Account

function Account.new(owner, balance)
    local self = setmetatable({}, Account)
    self.owner = owner
    self.balance = balance or 0
    self.history = {}
    return self
end

function Account:deposit(amount)
    self.balance = self.balance + amount
    self.history[#self.history + 1] = "+" .. amount
    return self
end

function Account:withdraw(amount)
    if amount > self.balance then
        self.history[#self.history + 1] = "denied " .. amount
        return false
    end
    self.balance -= amount
    self.history[#self.history + 1] = "-" .. amount
    return true
end

function Account:report()
    return self.owner .. ": " .. self.balance .. " [" .. table.concat(self.history, ", ") .. "]"
end

local acct = Account.new("alice", 100)
acct:deposit(50):deposit(25)
print(acct:withdraw(500), acct:withdraw(75))
print(acct:report())

local other = Account.new("bob")
other:deposit(1)
print(other:report(), acct.balance, other.balance)

local Savings = setmetatable({}, {__index = Account})
Savings.__index = Savings

function Savings.new(owner, balance, rate)
    local self = Account.new(owner, balance)
    self.rate = rate
    return setmetatable(self, Savings)
end

function Savings:accrue(years)
    for _ = 1, years do
        self:deposit(math.floor(self.balance * self.rate))
    end
end

local sv = Savings.new("carol", 1000, 0.1)
sv:accrue(3)
print(sv:report())
print(sv:withdraw(10), sv.balance)

local Vec = {}
Vec.__index = Vec
function Vec.new(x, y)
    return setmetatable({x = x, y = y}, Vec)
end
function Vec:len2()
    return self.x * self.x + self.y * self.y
end
function Vec:scaled(k)
    return Vec.new(self.x * k, self.y * k)
end
function Vec:str()
    return "(" .. self.x .. "," .. self.y .. ")"
end
local v = Vec.new(3, 4)
print(v:len2(), v:scaled(2):str(), v:scaled(2):scaled(0.5):len2())

local s = "Hello"
print(s:upper(), s:lower(), s:len(), s:sub(2, 3), ("x"):rep(3), ("%d|%s"):format(5, "y"))

local obj = {
    value = 10,
    get = function(self)
        return self.value
    end,
}
function obj:set(v)
    self.value = v
end
function obj.static(a)
    return a * 2
end
obj:set(obj:get() + 5)
print(obj:get(), obj.get(obj), obj.static(4))

local ns = {inner = {deep = {}}}
function ns.inner.deep.fn(a, b)
    return a - b
end
function ns.inner.deep:method(a)
    return self == ns.inner.deep, a
end
print(ns.inner.deep.fn(10, 3), ns.inner.deep:method("m"))

local list = {items = {}}
function list:push(...)
    for i = 1, select("#", ...) do
        self.items[#self.items + 1] = select(i, ...)
    end
    return #self.items
end
print(list:push(1, 2), list:push(3), table.concat(list.items, ""))

local key = "dynamic"
local dyn = {[key] = function(self, x)
    return x .. "!"
end}
print(dyn[key](dyn, "called"), dyn:dynamic("method"))
