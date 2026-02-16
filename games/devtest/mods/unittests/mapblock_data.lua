-- Tests for core.get_node_counts_in_area()

-- Test 1: Check counting nodes in an area
local function test_get_node_counts_in_area_basic(_, pos)
	-- Define a test area
	local minp = {x=pos.x, y=pos.y, z=pos.z}
	local maxp = {x=pos.x+5, y=pos.y+5, z=pos.z+5}
	
	-- Set some nodes to specific types
	core.set_node({x=pos.x, y=pos.y, z=pos.z}, {name="basenodes:stone"})
	core.set_node({x=pos.x+1, y=pos.y, z=pos.z}, {name="basenodes:stone"})
	core.set_node({x=pos.x+2, y=pos.y, z=pos.z}, {name="basenodes:dirt"})
	core.set_node({x=pos.x+3, y=pos.y, z=pos.z}, {name="air"})
	
	-- Get counts for specific node types
	local counts = core.get_node_counts_in_area(minp, maxp, {"basenodes:stone", "basenodes:dirt", "air"})
	assert(counts ~= nil, "get_node_counts_in_area should return counts")
	assert(type(counts) == "table", "get_node_counts_in_area should return a table")
	
	-- Verify counts
	assert(counts["basenodes:stone"] >= 2, "Should have at least 2 stone nodes")
	assert(counts["basenodes:dirt"] >= 1, "Should have at least 1 dirt node")
end
unittests.register("test_get_node_counts_in_area_basic", test_get_node_counts_in_area_basic, {map=true})

-- Test 2: Verify counts are accurate in a controlled area
local function test_get_node_counts_in_area_accurate(_, pos)
	-- Define a small test area
	local minp = {x=pos.x, y=pos.y, z=pos.z}
	local maxp = {x=pos.x+2, y=pos.y+2, z=pos.z+2}
	
	-- Clear area to air first
	for x = minp.x, maxp.x do
		for y = minp.y, maxp.y do
			for z = minp.z, maxp.z do
				core.set_node({x=x, y=y, z=z}, {name="air"})
			end
		end
	end
	
	-- Set exactly 5 stone nodes
	core.set_node({x=pos.x, y=pos.y, z=pos.z}, {name="basenodes:stone"})
	core.set_node({x=pos.x+1, y=pos.y, z=pos.z}, {name="basenodes:stone"})
	core.set_node({x=pos.x+2, y=pos.y, z=pos.z}, {name="basenodes:stone"})
	core.set_node({x=pos.x, y=pos.y+1, z=pos.z}, {name="basenodes:stone"})
	core.set_node({x=pos.x, y=pos.y, z=pos.z+1}, {name="basenodes:stone"})
	
	-- Get counts
	local counts = core.get_node_counts_in_area(minp, maxp, {"basenodes:stone", "air"})
	assert(counts ~= nil, "Should return counts")
	
	-- Verify exact counts (3x3x3 = 27 nodes, 5 stone, 22 air)
	assert(counts["basenodes:stone"] == 5, "Should have exactly 5 stone nodes")
	assert(counts["air"] == 22, "Should have exactly 22 air nodes")
end
unittests.register("test_get_node_counts_in_area_accurate", test_get_node_counts_in_area_accurate, {map=true})

-- Test 3: Test with single node name string instead of table
local function test_get_node_counts_in_area_single_node(_, pos)
	local minp = {x=pos.x, y=pos.y, z=pos.z}
	local maxp = {x=pos.x+2, y=pos.y+2, z=pos.z+2}
	
	-- Clear area and set some dirt nodes
	for x = minp.x, maxp.x do
		for y = minp.y, maxp.y do
			for z = minp.z, maxp.z do
				core.set_node({x=x, y=y, z=z}, {name="air"})
			end
		end
	end
	
	core.set_node({x=pos.x, y=pos.y, z=pos.z}, {name="basenodes:dirt"})
	core.set_node({x=pos.x+1, y=pos.y, z=pos.z}, {name="basenodes:dirt"})
	core.set_node({x=pos.x+2, y=pos.y, z=pos.z}, {name="basenodes:dirt"})
	
	-- Test with single node name (string instead of table)
	local counts = core.get_node_counts_in_area(minp, maxp, "basenodes:dirt")
	assert(counts ~= nil, "Should return counts for single node name")
	assert(counts["basenodes:dirt"] == 3, "Should have exactly 3 dirt nodes")
end
unittests.register("test_get_node_counts_in_area_single_node", test_get_node_counts_in_area_single_node, {map=true})

