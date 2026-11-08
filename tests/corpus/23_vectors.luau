local function show(v)
    return v.x .. "," .. v.y .. "," .. v.z
end

local a = vector.create(1, 2, 3)
local b = vector.create(-4.5, 0.25, 8)
local c = vector.create(0.1, 0.2, 0.3)
print(show(a), show(b), show(c))
print(show(a + b), show(a - b), show(a * 2), show(b / 2), show(a * b), show(-a))
print(show(vector.create(1, 2, 3) + vector.create(4, 5, 6)))
print(a.x + a.y + a.z, b.X, c.Y > 0.19)
print(type(a), typeof(a), a == vector.create(1, 2, 3), a ~= b)

local function make(x)
    return vector.create(x, x * 2, x * 3)
end
local sum = vector.zero
for i = 1, 5 do
    sum += make(i)
end
print(show(sum))

print(vector.magnitude(vector.create(3, 4, 0)), vector.dot(a, a), show(vector.cross(vector.create(1, 0, 0), vector.create(0, 1, 0))))
print(show(vector.floor(b)), show(vector.ceil(b)), show(vector.abs(b)))
print(show(vector.one), show(vector.zero), show(vector.create(1e10, -0, 7.75)))

local points = {vector.create(1, 1, 0), vector.create(-2, 3, 1), vector.create(0.5, -0.5, 2)}
local centroid = vector.zero
for _, p in ipairs(points) do
    centroid += p
end
centroid /= #points
print(show(centroid))

local scaled = {}
for i, p in ipairs(points) do
    scaled[i] = show(p * i + vector.create(0, 0, 1))
end
print(table.concat(scaled, " | "))

local folded = vector.create(1, 2, 3) * vector.create(2, 2, 2) - vector.create(0.5, 0.5, 0.5)
print(show(folded))

local constDouble = vector.create(0.1, 1 / 3, 123456.789)
print(show(constDouble))

local function lengthSquared(v)
    return v.x * v.x + v.y * v.y + v.z * v.z
end
print(lengthSquared(a), lengthSquared(vector.create(2, 0, 0)))
