local function quicksort(t, lo, hi)
    lo = lo or 1
    hi = hi or #t
    if lo < hi then
        local pivot = t[hi]
        local i = lo - 1
        for j = lo, hi - 1 do
            if t[j] <= pivot then
                i += 1
                t[i], t[j] = t[j], t[i]
            end
        end
        t[i + 1], t[hi] = t[hi], t[i + 1]
        quicksort(t, lo, i)
        quicksort(t, i + 2, hi)
    end
    return t
end

local seed = 12345
local function rand(n)
    seed = (seed * 16807) % 2147483647
    return seed % n + 1
end

local data = {}
for i = 1, 40 do
    data[i] = rand(100)
end
print(table.concat(quicksort(data), " "))

local function binarySearch(t, value)
    local lo, hi = 1, #t
    while lo <= hi do
        local mid = (lo + hi) // 2
        local v = t[mid]
        if v == value then
            return mid
        elseif v < value then
            lo = mid + 1
        else
            hi = mid - 1
        end
    end
    return nil
end
print(binarySearch(data, data[17]) ~= nil, binarySearch(data, 1000), binarySearch({}, 1))

local function wordFrequency(text)
    local freq = {}
    for word in text:lower():gmatch("%a+") do
        freq[word] = (freq[word] or 0) + 1
    end
    local entries = {}
    for word, n in pairs(freq) do
        entries[#entries + 1] = {word = word, n = n}
    end
    table.sort(entries, function(x, y)
        if x.n ~= y.n then
            return x.n > y.n
        end
        return x.word < y.word
    end)
    local out = {}
    for i = 1, math.min(5, #entries) do
        out[i] = entries[i].word .. "=" .. entries[i].n
    end
    return table.concat(out, " ")
end
print(wordFrequency("the cat and the hat and the bat sat on the mat with a cat"))

local function permutations(list)
    local results = {}
    local function permute(arr, n)
        if n == 1 then
            results[#results + 1] = table.concat(arr)
            return
        end
        for i = 1, n do
            arr[i], arr[n] = arr[n], arr[i]
            permute(arr, n - 1)
            arr[i], arr[n] = arr[n], arr[i]
        end
    end
    permute(list, #list)
    table.sort(results)
    return results
end
local perms = permutations({"a", "b", "c", "d"})
print(#perms, perms[1], perms[12], perms[24])

local function matmul(m1, m2)
    local result = {}
    for i = 1, #m1 do
        result[i] = {}
        for j = 1, #m2[1] do
            local s = 0
            for k = 1, #m2 do
                s += m1[i][k] * m2[k][j]
            end
            result[i][j] = s
        end
    end
    return result
end
local m = {{1, 2}, {3, 4}}
local power = {{1, 0}, {0, 1}}
for _ = 1, 10 do
    power = matmul(power, m)
end
print(power[1][1], power[1][2], power[2][1], power[2][2])

local function bfs(graph, start)
    local visited = {[start] = true}
    local queue = {start}
    local order = {}
    local head = 1
    while head <= #queue do
        local node = queue[head]
        head += 1
        order[#order + 1] = node
        for _, nb in ipairs(graph[node] or {}) do
            if not visited[nb] then
                visited[nb] = true
                queue[#queue + 1] = nb
            end
        end
    end
    return table.concat(order, "->")
end
local graph = {a = {"b", "c"}, b = {"d"}, c = {"d", "e"}, d = {"f"}, e = {"f"}, f = {}}
print(bfs(graph, "a"), bfs(graph, "e"), bfs(graph, "z"))

local function memoize(fn)
    local cache = {}
    local hits = 0
    return function(n)
        local v = cache[n]
        if v ~= nil then
            hits += 1
            return v
        end
        v = fn(n)
        cache[n] = v
        return v
    end, function()
        return hits
    end
end
local slowSquare = function(n)
    local s = 0
    for _ = 1, n do
        s += n
    end
    return s
end
local fastSquare, getHits = memoize(slowSquare)
local squares = {}
for i = 1, 20 do
    squares[#squares + 1] = fastSquare(i % 7)
end
print(table.concat(squares, ","), getHits())

local function rle(s)
    local out = {}
    local i = 1
    while i <= #s do
        local ch = s:sub(i, i)
        local j = i
        repeat
            j += 1
        until s:sub(j, j) ~= ch
        out[#out + 1] = (j - i > 1 and tostring(j - i) or "") .. ch
        i = j
    end
    return table.concat(out)
end
print(rle("aaabccddddde"), rle(""), rle("x"))
