local sw = os.clock()
for _ = 1, 1000000 do
    local r = {
        a = 1,  b = 2,  c = 3,  d = 4,
        e = 5,  f = 6,  g = 7,  h = 8,
        i = 9,  j = 10, k = 11, l = 12,
        m = 13, n = 14, o = 15, p = 16,
        q = 17, r = 18, s = 19, t = 20
    }
end
local dw = os.clock() - sw

local swo = os.clock()
for _ = 1, 1000000 do
    local r = nil
end
local dwo = os.clock() - swo

print(
    string.format(
        "Average delay per table constructed: %sus",
        (dw - dwo )
    )
)
