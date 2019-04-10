function calln( n )
    if n > 0 then
        calln( n - 1 )
    end
end


local sw = os.clock()
for _ = 1,100 do
    calln( 10000 )
end
local dw = os.clock() - sw

local swo = os.clock()
for _ = 1,100 do
end
local dwo = os.clock() - swo

print(
    string.format(
        "Average delay per call: %sus",
        (dw - dwo)
    )
)
