-- Table to keep track of callback executions
-- [i + 0] = count of expected patterns of index (i + 1)
-- [i + 1] = pattern to check
local PATTERN_NORMAL = { 4, "allow_%w", 2, "on_take", 1, "on_put", 1 }
local PATTERN_SWAP   = { 8, "allow_%w", 4, "on_take", 2, "on_put", 2 }
local exec_listing = {} -- List of logged callbacks (e.g. "on_take", "allow_put")

-- Checks whether the logged callbacks equal the expected pattern
core.__helper_check_callbacks = function(expect_swap)
	local exec_pattern = expect_swap and PATTERN_SWAP or PATTERN_NORMAL
	local ok = #exec_listing == exec_pattern[1]
	if ok then
		local list_index = 1
		for i = 2, #exec_pattern, 2 do
			for n = 1, exec_pattern[i + 1] do
				-- Match the list for "n" occurrences of the wanted callback name pattern
				ok = exec_listing[list_index]:find(exec_pattern[i])
				list_index = list_index + 1
				if not ok then break end
			end
			if not ok then break end
		end
	end

	if not ok then
		print("Execution order mismatch!")
		print("Expected patterns: ", dump(exec_pattern))
		print("Got list: ", dump(exec_listing))
	end
	exec_listing = {}
	return ok
end

-- Uncomment the other line for easier callback debugging
local log = function(...) end
--local log = print

core.register_allow_player_inventory_action(function(_, action, inv, info)
	log("\tallow " .. action, info.count or info.stack:to_string())

	if action == "move" then
		-- testMoveFillStack
		return info.count
	end

	if action == "take" or action == "put" then
		assert(not info.stack:is_empty(), "Stack empty in: " .. action)

		-- testMoveUnallowed
		-- testSwapFromUnallowed
		-- testSwapToUnallowed
		if info.stack:get_name() == "default:takeput_deny" then
			return 0
		end

		-- testMovePartial
		if info.stack:get_name() == "default:takeput_max_5" then
			return 5
		end

		-- testCallbacks
		if info.stack:get_name():find("default:takeput_cb_%d") then
			-- Log callback as executed
			table.insert(exec_listing, "allow_" .. action)
			return -- Unlimited
		end
	end

	return -- Unlimited
end)

core.register_on_player_inventory_action(function(_, action, inv, info)
	log("\ton " .. action, info.count or info.stack:to_string())

	if action == "take" or action == "put" then
		assert(not info.stack:is_empty(), action)

		if info.stack:get_name():find("default:takeput_cb_%d") then
			-- Log callback as executed
			table.insert(exec_listing, "on_" .. action)
			return
		end
	end
end)
