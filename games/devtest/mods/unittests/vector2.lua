--
-- Vector2 engine API push/read test
--
-- This test verifies that the engine correctly pushes and reads 2D vectors
-- to and from Lua. It uses the spritediv property on player object properties,
-- which is a 2D vector in the engine (v2s16).
-- The test ensures that metatables are correctly set by vector2.check().
--
local function test_vector2_push_read(player)
	-- Get original properties to restore later
	local old_props = player:get_properties()
	
	-- Set a vector2 value via engine API (spritediv is a v2s16 in the engine)
	local test_vector = vector2.new(5, 8)
	player:set_properties({spritediv = test_vector})
	
	-- Read back the value from engine
	local props = player:get_properties()
	local retrieved_vector = props.spritediv
	
	-- Verify the engine correctly pushed a vector2 with proper metatable
	assert(vector2.check(retrieved_vector), "Retrieved spritediv is not a valid vector2")
	
	-- Verify the values are correct
	assert(retrieved_vector.x == 5, "spritediv.x should be 5")
	assert(retrieved_vector.y == 8, "spritediv.y should be 8")
	
	-- Test with a table (should be converted by engine)
	player:set_properties({spritediv = {x = 3, y = 7}})
	props = player:get_properties()
	retrieved_vector = props.spritediv
	
	-- Verify the engine converted the table to a proper vector2
	assert(vector2.check(retrieved_vector), "Retrieved spritediv from table is not a valid vector2")
	assert(retrieved_vector.x == 3, "spritediv.x should be 3")
	assert(retrieved_vector.y == 7, "spritediv.y should be 7")
	
	-- Restore original properties
	player:set_properties({spritediv = old_props.spritediv})
end

unittests.register("test_vector2_push_read", test_vector2_push_read, {player=true})
