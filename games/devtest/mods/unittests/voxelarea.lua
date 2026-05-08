
local function test_voxelarea_invalid_area_example(player, pos)
	local p1, p2 = vector.new(0,16,0), vector.new(15,31,15)
	local va = VoxelArea(p1, p2)
	local vmax = va:getVolume() -- this is 4096
	for i in va:iter(0, 16, 0, 15, 0, 15) do
		if i < 0 or i > vmax then
			error(("index %d is invalid"):format(i))
		end
	end
end

unittests.register("test_voxelarea_invalid_area_example", test_voxelarea_invalid_area_example, {})

-- Compare two (functions returning) iterators for equality.
-- We need to use functions returning iterators because
--		otherwise changing the iterator implementation
--		could change which of the 6 parameters are nil,
--		and how they line up.
local function compare_iterators(iterable_a, iterable_b)
	local fun_a, state_a, control_a = iterable_a()
	local fun_b, state_b, control_b = iterable_b()
	while true do
		control_a = fun_a(state_a, control_a)
		control_b = fun_b(state_b, control_b)
		if control_a == nil or control_b == nil then
			-- At least one iterator has ended.
			-- If they did not both end, then they differ in length.
			return control_a == control_b
		elseif control_a ~= control_b then
			-- The iterator elements differ.
			return false
		end
	end
end

-- These are equivalent:
--		`va:iter(0, 10, 0, 15, 20, 15)`
--		`va:iter(0, 16, 0, 15, 20, 15)`
local function test_voxelarea_equivalence_example(player, pos)
	local p1, p2 = vector.new(0,16,0), vector.new(15,31,15)
	local va = VoxelArea(p1, p2)

	local comparison_success = compare_iterators(
			function() return va:iter(0, 10, 0, 15, 20, 15) end,
			function() return va:iter(0, 16, 0, 15, 20, 15) end
		)
	if not comparison_success then
		error("Unexpected mismatch between VoxelArea iterators!")
	end

end
unittests.register("test_voxelarea_equivalence_example", test_voxelarea_equivalence_example, {})

local function empty_iterator()
	return function()
		return nil
	end
end

-- `va:iter(0, 10, 0, 15, 12, 15)` produces no results.
local function test_voxelarea_empty_example(player, pos)
	local p1, p2 = vector.new(0,16,0), vector.new(15,31,15)
	local va = VoxelArea(p1, p2)

	local comparison_success = compare_iterators(
			function() return va:iter(0, 10, 0, 15, 12, 15) end,
			empty_iterator
		)
	if not comparison_success then
		error("supposedly empty VoxelArea iterator was not empty")
	end
end

unittests.register("test_voxelarea_empty_example", test_voxelarea_empty_example, {})
