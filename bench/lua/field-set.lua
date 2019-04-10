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
    r.f1  = 1
    r.f2  = 2
    r.f3  = 3
    r.f4  = 4
    r.f5  = 5
    r.f6  = 6
    r.f7  = 7
    r.f8  = 8
    r.f9  = 9
    r.f10 = 10
    r.f11 = 11
    r.f12 = 12
    r.f13 = 13
    r.f14 = 14
    r.f15 = 15
    r.f16 = 16
    r.f17 = 17
    r.f18 = 18
    r.f19 = 19
    r.f20 = 20
    math.random()
end
local dw = os.clock() - sw

local swo = os.clock()
for _ = 1, 1000000 do
    math.random()
end
local dwo = os.clock() - swo

print(
    string.format(
        "Average delay per field set: %sus",
        (dw - dwo )/10
    )
)
