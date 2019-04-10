local r = {
    f1  = 1,
    f2  = 2,
    f3  = 3,
    f4  = 4,
    f5  = 5,
    f6  = 6,
    f7  = 7,
    f8  = 8,
    f9  = 9,
    f10 = 10,
    f11 = 11,
    f12 = 12,
    f13 = 13,
    f14 = 14,
    f15 = 15,
    f16 = 16,
    f17 = 17,
    f18 = 18,
    f19 = 19,
    f20 = 20
}

local sw = os.clock()
for _ = 1, 1000000 do
    _ = r.f1
    _ = r.f2
    _ = r.f3
    _ = r.f4
    _ = r.f5
    _ = r.f6
    _ = r.f7
    _ = r.f8
    _ = r.f9
    _ = r.f10
    _ = r.f11
    _ = r.f12
    _ = r.f13
    _ = r.f14
    _ = r.f15
    _ = r.f16
    _ = r.f17
    _ = r.f18
    _ = r.f19
    _ = r.f20
end
local dw = os.clock() - sw

local swo = os.clock()
for _ = 1, 1000000 do
    local _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
    _ = nil
end
local dwo = os.clock() - swo

print(
    string.format(
        "Average delay per field set: %sus",
        (dw - dwo )/20
    )
)
