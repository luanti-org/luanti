local S = core.get_translator("testeditor")
local F = core.formspec_escape

local function editor_to_string(editor)
	if type(editor) == "string" then
		return "\"" .. editor .. "\""
	elseif type(editor) == "table" then
		return tostring(dump(editor)):gsub("\n", "")
	else
		return tostring(editor)
	end
end

local editor_formspecs = {}

local function get_object_properties_form(read_data, playername)
	if not playername then return "" end
	local formspec = editor_formspecs[playername]
	formspec.index_to_key = {}
	local str = ""
	local datalist = {}
	for k,_ in pairs(read_data) do
		table.insert(datalist, k)
	end
	table.sort(datalist)
	for p=1, #datalist do
		local k = datalist[p]
		local v = read_data[k]
		local newline = ""
		newline = k .. " = "
		newline = newline .. editor_to_string(v)
		str = str .. F(newline)
		if p < #datalist then
			str = str .. ","
		end
		table.insert(formspec.index_to_key, k)
	end
	return str
end

local function editor_formspec(playername)
	local formspec = editor_formspecs[playername]
	local sel = formspec.selindex or ""
	local key = formspec.index_to_key[sel]
	local value = ""
	if formspec.data[key] then
		value = editor_to_string(formspec.data[key])
	end
	local title = formspec.title
	if not formspec.actual then
		title = S("@1 - NOT APPLIED CHANGES", title)
	end
	core.show_formspec(playername, "testeditor:editor",
		"size[11,9]"..
		"label[0,0;"..F(title).."]"..
		"textlist[0,0.5;11,6.5;editor_data;"..formspec.list..";"..sel..";false]"..
		"field[0.2,7.75;7,1;key;"..F(S("Key"))..";"..F(formspec.key).."]"..
		"field_close_on_enter[key;false]"..
		"button[8,7.5;3,1;submit_key;"..F(S("Add/Change key")).."]"..
		"field[0.2,8.75;8,1;value;"..F(S("Value"))..";"..F(value).."]"..
		"field_close_on_enter[value;false]"..
		"button[8,8.5;3,1;submit_value;"..F(S("Submit and apply")).."]"
	)
end

local function editor_formspec_create(playername, wrapper)
	local data = wrapper.read_cb(playername)
	editor_formspecs[playername] = {
		title = wrapper.title,
		read_cb = wrapper.read_cb,
		write_cb = wrapper.write_cb,
		data = data,
		key = "",
		actual = true,
	}
	editor_formspecs[playername].list = get_object_properties_form(data, playername)
	editor_formspec(playername)
end

-- Use loadstring to parse param as a Lua value
local function use_loadstring(param, player)
	-- For security reasons, require 'server' priv, just in case
	-- someone is actually crazy enough to run this on a public server.
	local privs = core.get_player_privs(player:get_player_name())
	if not privs.server then
		return false, "You need 'server' privilege to change object properties!"
	end
	if not param then
		return false, "Failed: parameter is nil"
	end
	--[[ DANGER ZONE ]]
	-- Interpret string as Lua value
	local func, errormsg = loadstring("return (" .. param .. ")")
	if not func then
		return false, "Failed: " .. errormsg
	end

	-- Apply sandbox here using setfenv
	setfenv(func, {})

	-- Run it
	local good, errOrResult = pcall(func)
	if not good then
		-- A Lua error was thrown
		return false, "Failed: " .. errOrResult
	end

	-- errOrResult will be the value
	return true, errOrResult
end

core.register_on_player_receive_fields(function(player, formname, fields)
	if not (player and player:is_player()) then
		return
	end
	if formname == "testeditor:editor" then
		local name = player:get_player_name()
		local formspec = editor_formspecs[name]
		if not formspec then
			return
		end
		if fields.editor_data then
			local expl = core.explode_textlist_event(fields.editor_data)
			if expl.type == "DCL" or expl.type == "CHG" then
				formspec.selindex = expl.index
				formspec.key = formspec.index_to_key[expl.index]
				editor_formspec(name)
				return
			end
		end
		if fields.key_enter_field == "key" or fields.submit_key then
			local success, str = use_loadstring(fields.value, player)
			if success then
				local key = fields.key
				formspec.data[key] = str
				formspec.list = get_object_properties_form(formspec.data, name)
				formspec.actual = false
			else
				core.chat_send_player(name, str)
				return
			end
			editor_formspec(name)
			if fields.submit_value then
				formspec.write_cb(name, formspec.data)
			end
			return
		end
		if fields.key_enter_field == "value" or fields.submit_value then
			local success, str = use_loadstring(fields.value, player)
			if success then
				local key = formspec.index_to_key[formspec.selindex]
				formspec.data[key] = str
				formspec.list = get_object_properties_form(formspec.data, name)
				formspec.actual = false
			else
				core.chat_send_player(name, str)
				return
			end
			editor_formspec(name)
			if fields.submit_value then
				formspec.write_cb(name, formspec.data)
				formspec.data = formspec.read_cb(name)
				formspec.list = get_object_properties_form(formspec.data, name)
				formspec.actual = true
			end
			return
		end
	end
end)

local function create_read_cb(func)
	return
		function(name)
			local player = core.get_player_by_name(name)
			if player then
				return player[func](player)
			end
			return {}
		end
end
local function create_write_cb(func)
	return
		function(name, data)
			local player = core.get_player_by_name(name)
			if player then
				return player[func](player, data)
			end
		end
end

local wrappers = {
	armor = {
		title = S("Properties editor of armor groups (get_armor_groups/set_armor_groups)"),
		read_cb = create_read_cb("get_armor_groups"),
		write_cb = create_write_cb("set_armor_groups")
	},
	nametag = {
		title = S("Properties editor of nametag (get_nametag/set_nametag)"),
		read_cb = create_read_cb("get_nametag_attributes"),
		write_cb = create_write_cb("set_nametag_attributes")
	},
	physics = {
		title = S("Properties editor of physics_override (get_physics_override/set_physics_override)"),
		read_cb = create_read_cb("get_physics_override"),
		write_cb = create_write_cb("set_physics_override")
	},
	hud_flags = {
		title = S("Properties editor of hud_flags (hud_get_flags/hud_set_flags)"),
		read_cb = create_read_cb("hud_get_flags"),
		write_cb = create_write_cb("hud_set_flags")
	},
	sky = {
		title = S("Properties editor of sky (get_sky/set_sky)"),
		read_cb =
			function(name)
				local player = core.get_player_by_name(name)
				if player then
					return player:get_sky(true)
				end
				return {}
			end,
		write_cb = create_write_cb("set_sky")
	},
	sun = {
		title = S("Properties editor of sun (get_sun/set_sun)"),
		read_cb = create_read_cb("get_sun"),
		write_cb = create_write_cb("set_sun")
	},
	moon = {
		title = S("Properties editor of moon (get_moon/set_moon)"),
		read_cb = create_read_cb("get_moon"),
		write_cb = create_write_cb("set_moon")
	},
	stars = {
		title = S("Properties editor of stars (get_stars/set_stars)"),
		read_cb = create_read_cb("get_stars"),
		write_cb = create_write_cb("set_stars")
	},
	clouds = {
		title = S("Properties editor of clouds (get_clouds/set_clouds)"),
		read_cb = create_read_cb("get_clouds"),
		write_cb = create_write_cb("set_clouds")
	},
	lighting = {
		title = S("Properties editor of lighting (get_lighting/set_lighting)"),
		read_cb = create_read_cb("get_lighting"),
		write_cb = create_write_cb("set_lighting")
	},
	flags = {
		title = S("Properties editor of flags (get_flags/set_flags)"),
		read_cb = create_read_cb("get_flags"),
		write_cb = create_write_cb("set_flags")
	}
}

local editor_params = ""
local sep = ""

for key, _ in pairs(wrappers) do
	editor_params = editor_params..sep..key
	sep = "|"
end

core.register_chatcommand("player_editor", {
	params = "<"..editor_params..">",
	description = "Open editor for some player data",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		if wrappers[param] then
			editor_formspec_create(name, wrappers[param])
		else
			return false, S("Use with @1.", editor_params)
		end
		return true
	end,
})

