-- Tests for core.get_node_content_counts()

-- Helper function to clear all nodes in a mapblock to air using voxel manipulator
local function clear_mapblock_to_air(blockpos)
	-- Convert blockpos to world coordinates
	-- A mapblock is 16x16x16, so multiply by 16 to get world position
	local minp = {
		x = blockpos.x * 16,
		y = blockpos.y * 16,
		z = blockpos.z * 16
	}
	local maxp = {
		x = minp.x + 15,
		y = minp.y + 15,
		z = minp.z + 15
	}
	
	-- Get voxel manipulator and load the area
	local vm = core.get_voxel_manip(minp, maxp)
	local data = vm:get_data()
	
	-- Set all nodes to air
	for index = 1, #data do
		data[index] = core.CONTENT_AIR
	end
	
	-- Write the changes back to the map
	vm:set_data(data)
	vm:write_to_map()
end

-- Test 1: Check getting counts for not loaded or unexistent mapblock
local function test_get_node_content_counts_unloaded(_, pos)
	local far_blockpos = {x=10000, y=10000, z=10000}
	local counts = core.get_node_content_counts(far_blockpos)
	assert(counts == nil, "get_node_content_counts should return nil for non-existent/ungenerated block")
end
unittests.register("test_get_node_content_counts_unloaded", test_get_node_content_counts_unloaded, {map=true})

-- Test 2: Load a block, get node counts, set some nodes, verify counts changed
local function test_get_node_content_counts_changes(_, pos)
	local blockpos = {
		x = math.floor(pos.x / 16),
		y = math.floor(pos.y / 16),
		z = math.floor(pos.z / 16)
	}
	
	-- Clear the mapblock to air to ensure clean environment
	clear_mapblock_to_air(blockpos)
	
	-- Get initial counts for the loaded block
	local counts_before = core.get_node_content_counts(blockpos)
	assert(counts_before ~= nil, "get_node_content_counts should return counts for loaded block")
	assert(type(counts_before) == "table", "get_node_content_counts should return a table")
	
	-- Set some nodes to specific types
	local pos1 = {x=pos.x, y=pos.y, z=pos.z}
	local pos2 = {x=pos.x+1, y=pos.y, z=pos.z}
	core.set_node(pos1, {name="air"})
	core.set_node(pos2, {name="basenodes:stone"})
	
	-- Get counts after setting nodes
	local counts_after = core.get_node_content_counts(blockpos)
	assert(counts_after ~= nil, "Block should still be loaded")
	
	-- Verify counts changed
	local stone_id = core.get_content_id("basenodes:stone")
	assert(counts_after[stone_id] ~= nil, "Stone should be present in the block")
	assert(counts_after[stone_id] > 0, "Stone count should be positive")
end
unittests.register("test_get_node_content_counts_changes", test_get_node_content_counts_changes, {map=true})

-- Test 3: Set all nodes to air, add 3 nodes, verify counts
local function test_get_node_content_counts_all_nodes(_, pos)
	local blockpos = {
		x = math.floor(pos.x / 16),
		y = math.floor(pos.y / 16),
		z = math.floor(pos.z / 16)
	}
	
	-- Clear the mapblock to air to ensure clean environment
	clear_mapblock_to_air(blockpos)
	
	-- Get counts after clearing - should only have air
	local counts_all_air = core.get_node_content_counts(blockpos)
	assert(counts_all_air ~= nil, "Block should be loaded")
	local air_id = core.get_content_id("air")
	assert(counts_all_air[air_id] == 4096, "All 4096 nodes should be air")
	
	-- Count number of unique content IDs (should be 1 - just air)
	local num_types_air = 0
	for _ in pairs(counts_all_air) do
		num_types_air = num_types_air + 1
	end
	assert(num_types_air == 1, "Should only have one content type (air)")
	
	-- Add 3 random non-air nodes within the same mapblock
	-- Calculate positions relative to the mapblock's base position
	local base_x = blockpos.x * 16
	local base_y = blockpos.y * 16
	local base_z = blockpos.z * 16
	local test_positions = {
		{x=base_x+5, y=base_y+5, z=base_z+5},
		{x=base_x+10, y=base_y+7, z=base_z+3},
		{x=base_x+2, y=base_y+12, z=base_z+8}
	}
	core.set_node(test_positions[1], {name="basenodes:stone"})
	core.set_node(test_positions[2], {name="basenodes:dirt"})
	core.set_node(test_positions[3], {name="basenodes:gravel"})
	
	-- Get counts with 3 non-air nodes
	local counts_with_nodes = core.get_node_content_counts(blockpos)
	assert(counts_with_nodes ~= nil, "Block should still be loaded")
	
	-- Verify the counts
	local stone_id = core.get_content_id("basenodes:stone")
	local dirt_id = core.get_content_id("basenodes:dirt")
	local gravel_id = core.get_content_id("basenodes:gravel")
	
	assert(counts_with_nodes[stone_id] == 1, "Should have 1 stone node")
	assert(counts_with_nodes[dirt_id] == 1, "Should have 1 dirt node")
	assert(counts_with_nodes[gravel_id] == 1, "Should have 1 gravel node")
	assert(counts_with_nodes[air_id] == 4093, "Should have 4093 air nodes (4096 - 3)")
	
	-- Count number of unique content IDs (should be 4 - air + 3 node types)
	local num_types_with_nodes = 0
	for _ in pairs(counts_with_nodes) do
		num_types_with_nodes = num_types_with_nodes + 1
	end
	assert(num_types_with_nodes == 4, "Should have 4 content types")
	
	-- Set the 3 nodes back to air
	core.set_node(test_positions[1], {name="air"})
	core.set_node(test_positions[2], {name="air"})
	core.set_node(test_positions[3], {name="air"})
	
	-- Get counts again - should be back to all air
	local counts_air_again = core.get_node_content_counts(blockpos)
	assert(counts_air_again ~= nil, "Block should still be loaded")
	assert(counts_air_again[air_id] == 4096, "All 4096 nodes should be air again")
	
	-- Count number of unique content IDs (should be 1 again - just air)
	local num_types_air_again = 0
	for _ in pairs(counts_air_again) do
		num_types_air_again = num_types_air_again + 1
	end
	assert(num_types_air_again == 1, "Should only have one content type (air) again")
end
unittests.register("test_get_node_content_counts_all_nodes", test_get_node_content_counts_all_nodes, {map=true})

