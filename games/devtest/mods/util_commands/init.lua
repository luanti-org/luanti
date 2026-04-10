if core.is_singleplayer() then
	local function format_result(success, ...)
		if success then
			local res = {}
			for i = 1, select("#", ...) do
				local v = select(i, ...)
				table.insert(res, dump(v))
			end
			if #res == 0 then
				return true, "No return values."
			end
			return true, "Return values: " .. table.concat(res, ",\n")
		end
		return false, "Error: " .. tostring((...))
	end

	local function make_env(name)
		local me = core.get_player_by_name(name)
		local here = me:get_pos()
		local testtools = rawget(_G, "testtools")
		return setmetatable({
			-- WorldEdit //lua compatibility
			name = name,
			player = me,
			pos = here:round(),
			-- luacmd compatibility
			myname = name,
			me = me,
			here = here,
			branded = testtools and testtools.get_branded_object,
			print = function(...)
				local t = {}
				for i = 1, select("#", ...) do
					local v = select(i, ...)
					t[i] = dump(v)
				end
				core.chat_send_player(name, "/lua: " .. table.concat(t, "\t"))
			end,
		}, {__index = _G})
	end

	core.register_chatcommand("lua", {
		params = "<code>",
		description = "Execute Lua code (singleplayer-only)",
		func = function(name, param)
			local func = loadstring("return " .. param)
			if not func then
				local err
				func, err = loadstring(param)
				if not func then
					return false, "Syntax error: " .. err
				end
			end
			setfenv(func, make_env(name))
			core.chat_send_player(name, "Executing /lua " .. param)
			return format_result(pcall(func))
		end,
	})
end

core.register_chatcommand("hotbar", {
	params = "<size>",
	description = "Set hotbar size",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local size = tonumber(param)
		if not size then
			return false, "Missing or incorrect size parameter!"
		end
		local ok = player:hud_set_hotbar_itemcount(size)
		if ok then
			return true
		else
			return false, "Invalid item count!"
		end
	end,
})

core.register_chatcommand("hp", {
	params = "<hp>",
	description = "Set your health",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local hp = tonumber(param)
		if not hp or core.is_nan(hp) or hp < 0 or hp > 65535 then
			return false, "Missing or incorrect hp parameter!"
		end
		player:set_hp(hp)
		return true
	end,
})

local s_infplace = core.settings:get("devtest_infplace")
if s_infplace == "true" then
	infplace = true
elseif s_infplace == "false" then
	infplace = false
else
	infplace = core.is_creative_enabled("")
end

core.register_chatcommand("infplace", {
	params = "",
	description = "Toggle infinite node placement",
	func = function(name, param)
		infplace = not infplace
		if infplace then
			core.chat_send_all("Infinite node placement enabled!")
			core.log("action", "Infinite node placement enabled")
		else
			core.chat_send_all("Infinite node placement disabled!")
			core.log("action", "Infinite node placement disabled")
		end
		return true
	end,
})

core.register_chatcommand("detach", {
	params = "[<radius>]",
	description = "Detach all objects nearby",
	func = function(name, param)
		local radius = tonumber(param)
		if type(radius) ~= "number" then
			radius = 8
		end
		if radius < 1 then
			radius = 1
		end
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local objs = core.get_objects_inside_radius(player:get_pos(), radius)
		local num = 0
		for o=1, #objs do
			if objs[o]:get_attach() then
				objs[o]:set_detach()
				num = num + 1
			end
		end
		return true, string.format("%d object(s) detached.", num)
	end,
})

core.register_chatcommand("use_tool", {
	params = "(dig <group> <leveldiff>) | (hit <damage_group> <time_from_last_punch>) [<uses>]",
	description = "Apply tool wear a number of times, as if it were used for digging",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local mode, group, level, uses = string.match(param, "([a-z]+) ([a-z0-9]+) (-?%d+) (%d+)")
		if not mode then
			mode, group, level = string.match(param, "([a-z]+) ([a-z0-9]+) (-?%d+)")
			uses = 1
		end
		if not mode or not group or not level then
			return false
		end
		if mode ~= "dig" and mode ~= "hit" then
			return false
		end
		local tool = player:get_wielded_item()
		local caps = tool:get_tool_capabilities()
		if not caps or tool:get_count() == 0 then
			return false, "No tool in hand."
		end
		local actual_uses = 0
		for u=1, uses do
			local wear = tool:get_wear()
			local dp
			if mode == "dig" then
				dp = core.get_dig_params({[group]=3, level=level}, caps, wear)
			else
				dp = core.get_hit_params({[group]=100}, caps, level, wear)
			end
			tool:add_wear(dp.wear)
			actual_uses = actual_uses + 1
			if tool:get_count() == 0 then
				break
			end
		end
		player:set_wielded_item(tool)
		if tool:get_count() == 0 then
			return true, string.format("Tool used %d time(s). "..
					"The tool broke after %d use(s).", uses, actual_uses)
		else
			local wear = tool:get_wear()
			return true, string.format("Tool used %d time(s). "..
					"Final wear=%d", uses, wear)
		end
	end,
})


-- Unlimited node placement
core.register_on_placenode(function(pos, newnode, placer, oldnode, itemstack)
	if placer and placer:is_player() then
		return infplace
	end
end)

-- Don't pick up if the item is already in the inventory
local old_handle_node_drops = core.handle_node_drops
function core.handle_node_drops(pos, drops, digger)
	if not digger or not digger:is_player() or not infplace then
		return old_handle_node_drops(pos, drops, digger)
	end
	local inv = digger:get_inventory()
	if inv then
		for _, item in ipairs(drops) do
			if not inv:contains_item("main", item, true) then
				inv:add_item("main", item)
			end
		end
	end
end

core.register_chatcommand("set_displayed_itemcount", {
	params = "(-s \"<string>\" [-c <color>]) | -a <alignment_num>",
	description = "Set the displayed itemcount of the wielded item",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		local item = player:get_wielded_item()
		local meta = item:get_meta()
		local flag1 = param:sub(1, 2)
		if flag1 == "-s" then
			if param:sub(3, 4) ~= " \"" then
				return false, "Error: Space and string with \"s expected after -s."
			end
			local se = param:find("\"", 5, true)
			if not se then
				return false, "Error: String with two \"s expected after -s."
			end
			local s = param:sub(5, se - 1)
			if param:sub(se + 1, se + 4) == " -c " then
				s = core.colorize(param:sub(se + 5), s)
			end
			meta:set_string("count_meta", s)
		elseif flag1 == "-a" then
			local num = tonumber(param:sub(4))
			if not num then
				return false, "Error: Invalid number: "..param:sub(4)
			end
			meta:set_int("count_alignment", num)
		else
			return false
		end
		player:set_wielded_item(item)
		return true, "Displayed itemcount set."
	end,
})

core.register_chatcommand("dump_item", {
	params = "",
	description = "Prints a dump of the wielded item in table form",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		local item = player:get_wielded_item()
		local str = dump(item:to_table())
		print(str)
		return true, str
	end,
})

core.register_chatcommand("dump_itemdef", {
	params = "",
	description = "Prints a dump of the wielded item's definition in table form",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		local str = dump(player:get_wielded_item():get_definition())
		print(str)
		return true, str
	end,
})

core.register_chatcommand("dump_wear_bar", {
	params = "",
	description = "Prints a dump of the wielded item's wear bar parameters in table form",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		local item = player:get_wielded_item()
		local str = dump(item:get_wear_bar_params())
		print(str)
		return true, str
	end,
})

core.register_chatcommand("mapblock_stats", {
	params = "",
	description = "Prints counts of loadable, loaded, and active mapblocks",
	func = function(name, param)
		local loadable = core.get_loadable_blocks()
		local loaded = core.get_loaded_blocks()
		local active = core.get_active_blocks()
		return true, ("Loadable mapblocks: %d\nLoaded mapblocks: %d\nActive mapblocks: %d")
				:format(#loadable, #loaded, #active)
	end,
})

local dirt_with_grass = core.get_content_id("basenodes:dirt_with_grass")
local dirt_with_snow = core.get_content_id("basenodes:dirt_with_snow")

local function swap_grass_and_snow_in_mapblock(blockpos)
	local minp = blockpos * core.MAP_BLOCKSIZE
	local maxp = minp + (core.MAP_BLOCKSIZE - 1)
	local vm = core.get_voxel_manip(minp, maxp)
	local data = vm:get_data()
	local changed_nodes = 0
	for i = 1, #data do
		if data[i] == dirt_with_grass then
			data[i] = dirt_with_snow
			changed_nodes = changed_nodes + 1
		elseif data[i] == dirt_with_snow then
			data[i] = dirt_with_grass
			changed_nodes = changed_nodes + 1
		end
	end
	if changed_nodes > 0 then
		vm:set_data(data)
		vm:write_to_map()
	end
	return changed_nodes
end

local function swap_grass_and_snow_in_mapblocks(blocks)
	local changed_blocks = 0
	local changed_nodes = 0
	for _, blockpos in ipairs(blocks) do
		local changed = swap_grass_and_snow_in_mapblock(blockpos)
		if changed > 0 then
			changed_blocks = changed_blocks + 1
			changed_nodes = changed_nodes + changed
		end
	end
	return changed_blocks, changed_nodes
end

local function register_mapblock_swap_command(cmd, source, block_getter)
	core.register_chatcommand(cmd, {
		params = "",
		description = "Swap basenodes:dirt_with_grass and basenodes:dirt_with_snow in " .. source .. " mapblocks",
		func = function(name, param)
			local blocks = block_getter()
			local changed_blocks, changed_nodes = swap_grass_and_snow_in_mapblocks(blocks)
			return true, ("Checked %d %s mapblocks, changed %d mapblock(s), swapped %d node(s)")
					:format(#blocks, source, changed_blocks, changed_nodes)
		end,
	})
end

register_mapblock_swap_command("mapblock_swap_grass_snow_active", "active", core.get_active_blocks)
register_mapblock_swap_command("mapblock_swap_grass_snow_loaded", "loaded", core.get_loaded_blocks)
register_mapblock_swap_command("mapblock_swap_grass_snow_loadable", "loadable", core.get_loadable_blocks)

core.register_chatcommand("set_saturation", {
	params = "<saturation>",
	description = "Set the saturation for current player.",
	func = function(player_name, param)
		local saturation = tonumber(param)
		core.get_player_by_name(player_name):set_lighting({saturation = saturation })
	end
})
