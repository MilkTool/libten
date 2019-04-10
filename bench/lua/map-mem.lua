local r = {}
for _ = 1,1000000 do
    r = {
        a = 1,
        b = 2,
        c = 3,
        d = 4,
        e = 5,
        f = 6,
        g = 7,
        
        r = r
    }
end
