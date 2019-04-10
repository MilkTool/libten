local sw = os.clock()
for _ = 1, 1000000 do
    local a, b, c, d = 1, 2, 3, 4
    local f = function() return a + b + c + d end
end
local dw = os.clock() - sw

local swo = os.clock()
for _ = 1, 1000000 do
    local a, b, c, d = 1, 2, 3, 4
    local f = nil
end
local dwo = os.clock() - swo

print(
    string.format(
        "Average delay per closure constructed: %sus",
        (dw - dwo )
    )
)
