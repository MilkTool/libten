local sw = os.clock()
for _ = 1, 1000000 do
    local r = { a = 1, b = 2, c = 3, d = 4, e = 5 }
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
