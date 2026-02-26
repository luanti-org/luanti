-- Test mod to verify core.loaded_blocks and core.active_blocks tracking
-- This mod monitors block activation/deactivation and verifies the tables are correct

local function count_blocks(block_table)
	local count = 0
	for _ in pairs(block_table) do
		count = count + 1
	end
	return count
end

-- Monitor block loaded events
core.register_on_block_loaded(function(blockpos)
	core.log("action", "[test_blocks] Block loaded: " .. core.pos_to_string(blockpos))
end)

-- Monitor block activated events
core.register_on_block_activated(function(blockpos)
	core.log("action", "[test_blocks] Block activated: " .. core.pos_to_string(blockpos))
end)

-- Monitor block deactivated events
core.register_on_block_deactivated(function(blockpos_list)
	core.log("action", "[test_blocks] " .. #blockpos_list .. " blocks deactivated")
	for _, pos in ipairs(blockpos_list) do
		core.log("action", "[test_blocks]   - " .. core.pos_to_string(pos))
	end
end)

-- Monitor block unloaded events
core.register_on_block_unloaded(function(blockpos_list)
	core.log("action", "[test_blocks] " .. #blockpos_list .. " blocks unloaded from memory")
	for _, pos in ipairs(blockpos_list) do
		core.log("action", "[test_blocks]   - " .. core.pos_to_string(pos))
	end
end)

-- Periodic check to verify loaded_blocks >= active_blocks
local check_timer = 0
core.register_globalstep(function(dtime)
	check_timer = check_timer + dtime
	if check_timer >= 5.0 then  -- Check every 5 seconds
		check_timer = 0
		
		local loaded_count = count_blocks(core.loaded_blocks)
		local active_count = count_blocks(core.active_blocks)
		
		core.log("action", string.format(
			"[test_blocks] Blocks status: %d loaded, %d active",
			loaded_count, active_count
		))
		
		-- Verify that all active blocks are also loaded
		for hash, _ in pairs(core.active_blocks) do
			if not core.loaded_blocks[hash] then
				core.log("error", "[test_blocks] BUG: Active block is not in loaded_blocks!")
			end
		end
		
		-- The key test: loaded_blocks should have >= active_blocks
		if loaded_count < active_count then
			core.log("error", string.format(
				"[test_blocks] BUG: More active blocks (%d) than loaded blocks (%d)!",
				active_count, loaded_count
			))
		elseif loaded_count == active_count and loaded_count > 0 then
			core.log("warning", string.format(
				"[test_blocks] WARNING: Same number of loaded and active blocks (%d). " ..
				"This may indicate the old bug if view_range > active_block_range.",
				loaded_count
			))
		else
			core.log("action", "[test_blocks] OK: loaded_blocks >= active_blocks")
		end
	end
end)

core.log("action", "[test_blocks] Test mod initialized. Monitoring block tracking tables.")
