
local function test_voxelarea_invalid_area_iteration()
	local p1, p2 = vector.new(0,16,0), vector.new(15,31,15)
	local va = VoxelArea(p1, p2)
	-- This is 4096.
	local vmax = va:getVolume()
	for i in va:iter(0, 16, 0, 15, 0, 15) do
		if i < 0 or i > vmax then
			error(("VoxelArea index %d is invalid."):format(i))
		end
	end
end

unittests.register("test_voxelarea_invalid_area_iteration", test_voxelarea_invalid_area_iteration, {})

-- Pack 3 items into a closure.
-- Used to pack an iterator for `compare_iterators()`.
local function pack_triple(a, b, c)
	return function() return a, b, c end
end

-- Compare two closure-packed iterators for equality.
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
local function test_voxelarea_clipped_iterator_equivalence()
	local p1, p2 = vector.new(0,16,0), vector.new(15,31,15)
	local va = VoxelArea(p1, p2)

	local comparison_success = compare_iterators(
			pack_triple(va:iter(0, 10, 0, 15, 20, 15)),
			pack_triple(va:iter(0, 16, 0, 15, 20, 15))
		)
	if not comparison_success then
		error("Unexpected mismatch between clipped and non-clipped VoxelArea iterators!")
	end
end
unittests.register("test_voxelarea_clipped_iterator_equivalence", test_voxelarea_clipped_iterator_equivalence, {})

-- `va:iter(0, 10, 0, 15, 12, 15)` produces no results.
local function test_voxelarea_empty_out_of_bounds_iterator()
	local p1, p2 = vector.new(0,16,0), vector.new(15,31,15)
	local va = VoxelArea(p1, p2)

	local comparison_success = compare_iterators(
			pack_triple(va:iter(0, 10, 0, 15, 12, 15)),
			pack_triple(function() return nil end)
		)
	if not comparison_success then
		error("Out-of-bounds VoxelArea iterator was not empty.")
	end
end
unittests.register("test_voxelarea_empty_out_of_bounds_iterator", test_voxelarea_empty_out_of_bounds_iterator, {})
