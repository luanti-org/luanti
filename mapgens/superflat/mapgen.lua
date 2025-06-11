local stone = core.get_content_id("mapgen_stone")

core.register_on_generated(function(vm, minp, maxp, seed)
    local emin, emax = vm:get_emerged_area()
    local area = VoxelArea(emin, emax)
    local data = vm:get_data()

    local ly = 0
    for y = minp.y, maxp.y do
        ly = ly + 1
        local lz = 0
        for z = minp.z, maxp.z do
            lz = lz + 1
            local vi = area:index(minp.x, y, z)
            local lx = 0
            for x = minp.x, maxp.x do
                if y == -1 then
                    data[vi] = stone
                end
                vi = vi + 1
            end
        end
    end

    vm:set_data(data)
end)