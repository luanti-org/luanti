local player_font_huds = {}

local font_states = {
	{0, "Normal font"},
	{1, "Bold font"},
	{2, "Italic font"},
	{3, "Bold and italic font"},
	{4, "Monospace font"},
	{5, "Bold and monospace font"},
	{7, "ZOMG all the font styles"},
	{7, "Colors test! " .. core.colorize("green", "Green") ..
		core.colorize("red", "\nRed") .. " END"},
}


local font_default_def = {
	type = "text",
	position = {x = 0.5, y = 0.5},
	scale = {x = 2, y = 2},
	alignment = { x = 0, y = 0 },
	number = 0xFFFFFF,
}

local function add_font_hud(player, state)
	local def = table.copy(font_default_def)
	local statetbl = font_states[state]
	def.offset = {x = 0, y = 32 * state}
	def.style = statetbl[1]
	def.text = statetbl[2]
	return player:hud_add(def)
end

local font_etime = 0
local font_state = 0

core.register_globalstep(function(dtime)
	font_etime = font_etime + dtime
	if font_etime < 1 then
		return
	end
	font_etime = 0
	for _, player in ipairs(core.get_connected_players()) do
		local huds = player_font_huds[player:get_player_name()]
		if huds then
			for i, hud_id in ipairs(huds) do
				local statetbl = font_states[(font_state + i) % #font_states + 1]
				player:hud_change(hud_id, "style", statetbl[1])
				player:hud_change(hud_id, "text", statetbl[2])
			end
		end
	end
	font_state = font_state + 1
end)

core.register_chatcommand("hudfonts", {
	params = "[<HUD elements>]",
	description = "Show/Hide some text on the HUD with various font options",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		local param = tonumber(param) or 0
		param = math.min(math.max(param, 1), #font_states)
		if player_font_huds[name] == nil then
			player_font_huds[name] = {}
			for i = 1, param do
				table.insert(player_font_huds[name], add_font_hud(player, i))
			end
			core.chat_send_player(name, ("%d text HUD element(s) added."):format(param))
		else
			local huds = player_font_huds[name]
			if huds then
				for _, hud_id in ipairs(huds) do
					player:hud_remove(hud_id)
				end
				core.chat_send_player(name, "All text HUD elements removed.")
			end
			player_font_huds[name] = nil
		end
		return true
	end,
})

-- Testing waypoint capabilities

local player_waypoints = {}
core.register_chatcommand("hudwaypoints", {
	params = "[ add | add_change | remove ]",
	description = "Create HUD waypoints at your position for testing (add: Add waypoints and change them after 0.5s (default). add_change: Add waypoints and change immediately. remove: Remove all waypoints)",
	func = function(name, params)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		if params == "remove" then
			if player_waypoints[name] then
				for i=1, #player_waypoints[name] do
					player:hud_remove(player_waypoints[name][i])
				end
				player_waypoints[name] = {}
			end
			return true, "All waypoint HUD elements removed."
		end
		if not (params == "add_change" or params == "add" or params == "") then
			-- Incorrect syntax
			return false
		end
		local regular = player:hud_add {
			type = "waypoint",
			name = "regular waypoint",
			text = "m",
			number = 0xFFFFFF,
			world_pos = vector.add(player:get_pos(), {x = 0, y = 1.5, z = 0})
		}
		local reduced_precision = player:hud_add {
			type = "waypoint",
			name = "imprecise waypoint",
			text = "m (0.1 steps, precision = 10)",
			precision = 10,
			number = 0xFFFFFF,
			world_pos = vector.add(player:get_pos(), {x = 0, y = 1, z = 0})
		}
		local hidden_distance = player:hud_add {
			type = "waypoint",
			name = "waypoint with hidden distance",
			text = "this text is hidden as well (precision = 0)",
			precision = 0,
			number = 0xFFFFFF,
			world_pos = vector.add(player:get_pos(), {x = 0, y = 0.5, z = 0})
		}
		local function change(chplayer)
			if not (chplayer and chplayer:is_player()) then
				return
			end
			if regular then
				chplayer:hud_change(regular, "world_pos", vector.add(player:get_pos(), {x = 0, y = 3, z = 0}))
				chplayer:hud_change(regular, "number", 0xFF0000)
			end
			if reduced_precision then
				chplayer:hud_change(reduced_precision, "precision", 2)
				chplayer:hud_change(reduced_precision, "text", "m (0.5 steps, precision = 2)")
				chplayer:hud_change(reduced_precision, "number", 0xFFFF00)
			end
			if hidden_distance then
				chplayer:hud_change(hidden_distance, "number", 0x0000FF)
			end
			core.chat_send_player(chplayer:get_player_name(), "Waypoints changed.")
		end
		if params == "add_change" then
			-- change immediate
			change(player)
		else
			core.after(0.5, change, player)
		end
		local image_waypoint = player:hud_add {
			type = "image_waypoint",
			text = "testhud_waypoint.png",
			world_pos = player:get_pos(),
			-- 20% of screen width, 3x image height
			scale = {x = -20, y = 3},
			offset = {x = 0, y = -32}
		}
		if not player_waypoints[name] then
			player_waypoints[name] = {}
		end
		if regular then
			table.insert(player_waypoints[name], regular)
		end
		if reduced_precision then
			table.insert(player_waypoints[name], reduced_precision)
		end
		if hidden_distance then
			table.insert(player_waypoints[name], hidden_distance)
		end
		if image_waypoint then
			table.insert(player_waypoints[name], image_waypoint)
		end
		regular = regular or "error"
		reduced_precision = reduced_precision or "error"
		hidden_distance = hidden_distance or "error"
		image_waypoint = image_waypoint or "error"
		return true, "Waypoints added. IDs: regular: " .. regular .. ", reduced precision: " .. reduced_precision ..
			", hidden distance: " .. hidden_distance .. ", image waypoint: " .. image_waypoint
	end
})

core.register_on_joinplayer(function(player)
	player:set_properties({zoom_fov = 15})
end)

core.register_chatcommand("zoomfov", {
	params = "[<FOV>]",
	description = "Set or display your zoom_fov",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		if param == "" then
			local fov = player:get_properties().zoom_fov
			return true, "zoom_fov = "..tostring(fov)
		end
		local fov = tonumber(param)
		if not fov then
			return false, "Missing or incorrect zoom_fov parameter!"
		end
		player:set_properties({zoom_fov = fov})
		fov = player:get_properties().zoom_fov
		return true, "zoom_fov = "..tostring(fov)
	end,
})

-- Hotbars

local hud_hotbar_defs = {
	{
		type = "hotbar",
		position = {x=0.2, y=0.5},
		direction = 0,
		alignment = {x=1, y=-1},
	},
	{
		type = "hotbar",
		position = {x=0.2, y=0.5},
		direction = 2,
		alignment = {x=1, y=1},
	},
	{
		type = "hotbar",
		position = {x=0.7, y=0.5},
		direction = 0,
		offset = {x=140, y=20},
		alignment = {x=-1, y=-1},
	},
	{
		type = "hotbar",
		position = {x=0.7, y=0.5},
		direction = 2,
		offset = {x=140, y=20},
		alignment = {x=-1, y=1},
	},
}


local player_hud_hotbars= {}
core.register_chatcommand("hudhotbars", {
	description = "Shows some test Lua HUD elements of type hotbar. " ..
			"(add: Adds elements (default). remove: Removes elements)",
	params = "[ add | remove ]",
	func = function(name, params)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end

		local id_table = player_hud_hotbars[name]
		if not id_table then
			id_table = {}
			player_hud_hotbars[name] = id_table
		end

		if params == "remove" then
			for _, id in ipairs(id_table) do
				player:hud_remove(id)
			end
			return true, "Hotbars removed."
		end

		-- params == "add" or default
		for _, def in ipairs(hud_hotbar_defs) do
			table.insert(id_table, player:hud_add(def))
		end
		return true, #hud_hotbar_defs .." Hotbars added."
	end
})

-- Inventories

local hud_inventory_defs = {
	{
		type = "inventory",
		position = {x=0.2, y=0.5},
		direction = 0,
		text = "main",
		number = 4,
		item = 2,
	},
	{
		type = "inventory",
		position = {x=0.2, y=0.5},
		direction = 2,
		text = "main",
		number = 4,
		item = 2,
	},
	{
		type = "inventory",
		position = {x=0.7, y=0.5},
		direction = 1,
		text = "main",
		number = 4,
		item = 2,
	},
	{
		type = "inventory",
		position = {x=0.7, y=0.5},
		direction = 3,
		text = "main",
		number = 4,
		item = 2,
	},
}

local player_hud_inventories= {}
core.register_chatcommand("hudinventories", {
	description = "Shows some test Lua HUD elements of type inventory. (add: Adds elements (default). remove: Removes elements)",
	params = "[ add | remove ]",
	func = function(name, params)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end

		local id_table = player_hud_inventories[name]
		if not id_table then
			id_table = {}
			player_hud_inventories[name] = id_table
		end

		if params == "remove" then
			for _, id in ipairs(id_table) do
				player:hud_remove(id)
			end
			return true, "HUD Inventories removed."
		end

		-- params == "add" or default
		for _, def in ipairs(hud_inventory_defs) do
			table.insert(id_table, player:hud_add(def))
		end
		return true, #hud_inventory_defs .." HUD Inventories added."
	end
})


core.register_on_leaveplayer(function(player)
	local playername = player:get_player_name()
	player_font_huds[playername] = nil
	player_waypoints[playername] = nil
	player_hud_hotbars[playername] = nil
	player_hud_inventories[playername] = nil
end)

core.register_chatcommand("hudprint", {
	description = "Writes all used Lua HUD elements into chat.",
	func = function(name, params)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end

		local s = "HUD elements:"
		for k, elem in pairs(player:hud_get_all()) do
			local ename = dump(elem.name)
			local etype = dump(elem.type)
			local epos = "{x="..elem.position.x..", y="..elem.position.y.."}"
			s = s.."\n["..k.."]  type = "..etype.." | name = "..ename.." | pos = ".. epos
		end

		return true, s
	end
})

local hud_flags = {"hotbar", "healthbar", "crosshair", "wielditem", "breathbar",
		"minimap", "minimap_radar", "basic_debug", "chat"}

core.register_chatcommand("hudtoggleflag", {
	description = "Toggles a hud flag.",
	params = "[ ".. table.concat(hud_flags, " | ") .." ]",
	func = function(name, params)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end

		local flags = player:hud_get_flags()
		if flags[params] == nil then
			return false, "Unknown hud flag."
		end

		flags[params] = not flags[params]
		player:hud_set_flags(flags)
		return true, "Flag \"".. params .."\" set to ".. tostring(flags[params]) .. "."
	end
})
