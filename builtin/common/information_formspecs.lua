local COLOR_BLUE = "#7AF"
local COLOR_GREEN = "#7F7"
local COLOR_GRAY = "#BBB"

local LIST_FORMSPEC = [[
		size[13,6.5]
		label[0,-0.1;%s]
		tablecolumns[color;tree;text;text]
		table[0,0.5;12.8,5.5;list;%s;0]
		button_exit[5,6;3,1;quit;%s]
	]]

local LIST_FORMSPEC_DESCRIPTION = [[
		size[13,9.65]
		label[0,-0.1;%s]
		tablecolumns[color;tree;text;text]
		table[0,0.5;12.8,4.5;list;%s;%i]
		box[0,5.05;12.8,1;]
		textarea[0.35,5.1;12.9,1;;;%s]
		field_close_on_enter[search_text;false]
		field[0.3,7.55;8.9,0.8;search_text;;%s]
		field_enter_after_edit[search_text;true]
		image_button[9.3,7.27;0.85,0.85;search.png;btn_search;]
		image_button[10.2,7.27;0.85,0.85;clear.png;btn_clear;]
		tooltip[btn_search;%s]
		tooltip[btn_clear;%s]
		button_exit[5,8.6;3,1;quit;%s]
	]]

local F = core.formspec_escape
local S = core.get_translator("__builtin")
local check_player_privs = core.check_player_privs

-- Mirrors format_help_line() in chatcommands.lua so /help <cmd> and double-click match
local function format_help_line(cmd, def)
	local cmd_marker = INIT == "client" and "." or "/"
	local msg = core.colorize("#00ffff", cmd_marker .. cmd)
	if def.params and def.params ~= "" then
		msg = msg .. " " .. def.params
	end
	if def.description and def.description ~= "" then
		msg = msg .. ": " .. def.description
	end
	return msg
end

-- CHAT COMMANDS FORMSPEC

local mod_cmds = {}

local function load_mod_command_tree()
	mod_cmds = {}

	for name, def in pairs(core.registered_chatcommands) do
		mod_cmds[def.mod_origin] = mod_cmds[def.mod_origin] or {}
		local cmds = mod_cmds[def.mod_origin]

		-- Could be simplified, but avoid the priv checks whenever possible
		cmds[#cmds + 1] = { name, def }
	end
	local sorted_mod_cmds = {}
	for modname, cmds in pairs(mod_cmds) do
		table.sort(cmds, function(a, b) return a[1] < b[1] end)
		sorted_mod_cmds[#sorted_mod_cmds + 1] = { modname, cmds }
	end
	table.sort(sorted_mod_cmds, function(a, b) return a[1] < b[1] end)
	mod_cmds = sorted_mod_cmds
end

core.after(0, load_mod_command_tree)

-- Returns cmds whose name contains `search`, flattened across all mods and sorted by cmd name
local function get_flat_matches(search)
	local search_lc = search:lower()
	local matches = {}
	for _, mod_data in ipairs(mod_cmds) do
		for _, cmd in ipairs(mod_data[2]) do
			if cmd[1]:lower():find(search_lc, 1, true) then
				matches[#matches + 1] = cmd
			end
		end
	end
	table.sort(matches, function(a, b) return a[1] < b[1] end)
	return matches
end

-- Per-player current search text for the help formspec.
local current_search = {}
local CLIENT_SEARCH_KEY = "client" -- for singleplayer

if INIT ~= "client" then
	core.register_on_leaveplayer(function(player)
		current_search[player:get_player_name()] = nil
	end)
end

local function build_chatcommands_formspec(name, sel, copy, search)
	local search_key = name or CLIENT_SEARCH_KEY
	if search == nil then
		search = current_search[search_key] or ""
	end
	current_search[search_key] = search

	local rows = {}
	rows[1] = "#FFF,0,"..F(S("Command"))..","..F(S("Parameters"))

	local description = S("For more information, click on "
		.. "any entry in the list.").. "\n" ..
		S("Double-click to copy the entry to the chat history.")

	-- Handle a matched entry: show its description, and copy it to chat if requested.
	local function handle_selected(cmds)
		description = cmds[2].description
		if search ~= "" then
			description = S("From mod: @1", cmds[2].mod_origin) .. "\n" .. description
		end
		if copy then
			local msg = format_help_line(cmds[1], cmds[2])
			if INIT == "client" then
				core.display_chat_message(msg)
			else
				core.chat_send_player(name, msg)
			end
		end
	end

	if search == "" then
		for i, data in ipairs(mod_cmds) do
			rows[#rows + 1] = COLOR_BLUE .. ",0," .. F(data[1]) .. ","
			for j, cmds in ipairs(data[2]) do
				local has_priv = INIT == "client" or check_player_privs(name, cmds[2].privs)
				rows[#rows + 1] = ("%s,1,%s,%s"):format(
					has_priv and COLOR_GREEN or COLOR_GRAY,
					cmds[1], F(cmds[2].params))
				if sel == #rows then
					handle_selected(cmds)
				end
			end
		end
	else -- filter and show only list of commands, flattened
		for _, cmds in ipairs(get_flat_matches(search)) do
			local has_priv = INIT == "client" or check_player_privs(name, cmds[2].privs)
			rows[#rows + 1] = ("%s,0,%s,%s"):format(
				has_priv and COLOR_GREEN or COLOR_GRAY,
				cmds[1], F(cmds[2].params))
			if sel == #rows then
				handle_selected(cmds)
			end
		end
	end

	return LIST_FORMSPEC_DESCRIPTION:format(
			F(S("Available commands: (see also: /help <cmd>)")),
			table.concat(rows, ","), sel or 0,
			F(description),
			F(search),
			F(S("Search")), F(S("Clear")),
			F(S("Close"))
		)
end


-- PRIVILEGES FORMSPEC

local function build_privs_formspec(name)
	local privs = {}
	for priv_name, def in pairs(core.registered_privileges) do
		privs[#privs + 1] = { priv_name, def }
	end
	table.sort(privs, function(a, b) return a[1] < b[1] end)

	local rows = {}
	rows[1] = "#FFF,0,"..F(S("Privilege"))..","..F(S("Description"))

	local player_privs = core.get_player_privs(name)
	for i, data in ipairs(privs) do
		rows[#rows + 1] = ("%s,0,%s,%s"):format(
			player_privs[data[1]] and COLOR_GREEN or COLOR_GRAY,
				data[1], F(data[2].description))
	end

	return LIST_FORMSPEC:format(
			F(S("Available privileges:")),
			table.concat(rows, ","),
			F(S("Close"))
		)
end


-- Returns the new search text if `fields` is a search/clear action, else nil
local function get_requested_search(fields)
	if fields.btn_clear then
		return ""
	end
	if fields.btn_search or fields.key_enter_field == "search_text" then
		return fields.search_text or ""
	end
	return nil
end

-- DETAILED CHAT COMMAND INFORMATION
if INIT == "client" then
	core.register_on_formspec_input(function(formname, fields)
		if formname ~= "__builtin:help_cmds" or fields.quit then
			return
		end

		local search = get_requested_search(fields)
		local event = core.explode_table_event(fields.list)
		if search ~= nil then
			core.show_formspec("__builtin:help_cmds",
				build_chatcommands_formspec(nil, nil, false, search))
		elseif event.type ~= "INV" then
			core.show_formspec("__builtin:help_cmds",
				build_chatcommands_formspec(nil, event.row, event.type == "DCL"))
		end
	end)
else
	core.register_on_player_receive_fields(function(player, formname, fields)
		if formname ~= "__builtin:help_cmds" or fields.quit then
			return
		end

		local name = player:get_player_name()
		local search = get_requested_search(fields)
		local event = core.explode_table_event(fields.list)
		if search ~= nil then
			core.show_formspec(name, "__builtin:help_cmds",
				build_chatcommands_formspec(name, nil, false, search))
		elseif event.type ~= "INV" then
			core.show_formspec(name, "__builtin:help_cmds",
				build_chatcommands_formspec(name, event.row, event.type == "DCL"))
		end
	end)
end

function core.show_general_help_formspec(name)
	if INIT == "client" then
		core.show_formspec("__builtin:help_cmds",
			build_chatcommands_formspec(nil, nil, false, ""))
	else
		core.show_formspec(name, "__builtin:help_cmds",
			build_chatcommands_formspec(name, nil, false, ""))
	end
end

if INIT ~= "client" then
	function core.show_privs_help_formspec(name)
		core.show_formspec(name, "__builtin:help_privs",
			build_privs_formspec(name))
	end
end
