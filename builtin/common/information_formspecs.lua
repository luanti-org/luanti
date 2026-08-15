local COLOR_CYAN = "#00ffff"
local COLOR_GREEN = "#7F7"
local COLOR_GRAY = "#BBB"

local LIST_FORMSPEC = [[
		size[13,6.5]
		label[0,-0.1;%s]
		tablecolumns[color;tree;text;text]
		table[0,0.5;12.8,5.5;list;%s;0]
		button_exit[5,6;3,1;quit;%s]
	]]

local F = core.formspec_escape
local S = core.get_translator("__builtin")
local check_player_privs = core.check_player_privs


-- CHAT COMMANDS FORMSPEC

local CMD_MARKER = INIT == "client" and "." or "/"

local MOD_LIST_NAME = "modlist"
local CMND_TABLE_NAME = "cmdtable"
local SEARCH_FIELD_NAME = "search"
local SEARCH_BTN_NAME = "searchbtn"
local CLEAR_BTN_NAME = "clearbtn"

local formsData = {}

local all_mods = {}
local function get_all_mod_names()
	if #all_mods > 0 then
		return all_mods
	end
	local seen = {}
	for _, def in pairs(core.registered_chatcommands) do
		local origin = def.mod_origin or "??"
		if not seen[origin] then
			seen[origin] = true
			all_mods[#all_mods + 1] = origin
		end
	end
	table.sort(all_mods)
	return all_mods
end

local command_per_mod = {}
local function get_all_commands_for_mod(modname)
	local modCmds = command_per_mod[modname]
	if not modCmds then
		modCmds = {}
		for cmd, def in pairs(core.registered_chatcommands) do
			if (def.mod_origin or "??") == modname then
				modCmds[#modCmds + 1] = { cmd, def }
			end
		end
		table.sort(modCmds, function(a, b) return a[1] < b[1] end)
		command_per_mod[modname] = modCmds
	end
	return modCmds
end

local function get_commands_for_mod(modname, term)
	local modCmds = get_all_commands_for_mod(modname)
	if term == "" then
		return modCmds
	end

	local lterm = term:lower()
	local cmds = {}
	for _, entry in ipairs(modCmds) do
		if entry[1]:lower():find(lterm, 1, true) then
			cmds[#cmds + 1] = entry
		end
	end
	return cmds
end

local function get_filtered_mods(search)
	local allMods = get_all_mod_names()
	if search == "" then
		return allMods
	end
	local lsearch = search:lower()
	local filteredMods = {}
	for _, m in ipairs(allMods) do
		if m:lower():find(lsearch, 1, true) or #get_commands_for_mod(m, lsearch) > 0 then
			filteredMods[#filteredMods + 1] = m
		end
	end
	return filteredMods
end

local function get_chatcommands_formspec(name)
	local data = formsData[name]
	if not data then
		data = { search = "" }
		formsData[name] = data
	end

	local filteredMods = get_filtered_mods(data.search)

	if not data.selectedMod or table.indexof(filteredMods, data.selectedMod) == -1 then
		data.selectedMod = filteredMods[1]
		data.selectedCmdIndex = nil
	end

	local modListItems = {}
	local selModIndex = 0
	for i, m in ipairs(filteredMods) do
		modListItems[i] = F(m)
		if m == data.selectedMod then
			selModIndex = i
		end
	end

	local commandRows = { "#FFF,"..F(S("Command"))..","..F(S("Parameters")) }
	local description = S("Select a command to see its description.")
	if data.selectedMod then
		local commands = get_commands_for_mod(data.selectedMod, data.search)
		for i, entry in ipairs(commands) do
			local cmdname, def = entry[1], entry[2]
			local hasPriv = INIT == "client" or check_player_privs(name, def.privs)
			local color = hasPriv and COLOR_GREEN or COLOR_GRAY
			commandRows[#commandRows + 1] = color..","..F(CMD_MARKER..cmdname)..","..F(def.params or "")
			if data.selectedCmdIndex == i then
				local header = CMD_MARKER..cmdname
				if def.params and def.params ~= "" then
					header = header.." "..def.params
				end
				description = header.."\n\n"..(def.description or "")
			end
		end
	end

	local modLabel = data.selectedMod and F(S("Commands from: @1", data.selectedMod))
		or F(S("No mods match your search"))

	return "formspec_version[4]"..
		"size[16.5,10]"..
		"label[0.2,0.3;"..F(S("Mods")).."]"..
		"textlist[0.2,0.6;4.3,7.9;"..
			MOD_LIST_NAME..";"..table.concat(modListItems, ",")..";"..selModIndex..";false]"..
		"field[0.2,8.6;2.6,0.8;"..
			SEARCH_FIELD_NAME..";;"..F(data.search).."]"..
		"field_close_on_enter["..SEARCH_FIELD_NAME..";false]"..
		"button[2.9,8.6;0.85,0.8;"..
			SEARCH_BTN_NAME..";>]"..
		"tooltip["..SEARCH_BTN_NAME..";"..F(S("Search")).."]"..
		"button[3.8,8.6;0.85,0.8;"..
			CLEAR_BTN_NAME..";X]"..
		"label[4.6,0.3;"..modLabel.."]"..
		"tablecolumns[color;text;text]"..
		"table[4.6,0.6;11.7,5.0;"..
			CMND_TABLE_NAME..";"..table.concat(commandRows, ",")..";"..
			(data.selectedCmdIndex and (data.selectedCmdIndex + 1) or 0).."]"..
		"textarea[4.6,5.9;11.7,3.6;;;"..F(description).."]"..
		"button_exit[14.5,9.1;1.8,0.7;quit;"..F(S("Close")).."]"
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


-- DETAILED CHAT COMMAND INFORMATION

function core.show_general_help_formspec(name)
	if INIT == "client" then
		core.show_formspec("__builtin:help_cmds", get_chatcommands_formspec(name))
	else
		core.show_formspec(name, "__builtin:help_cmds", get_chatcommands_formspec(name))
	end
end

-- return: true if formspec needs to be reshown
local function handle_help_cmds_fields(name, fields)
	local key = name or ""
	local data = formsData[key]
	if not data then
		return false
	end

	if fields.quit and not fields.key_enter_field then
		formsData[key] = nil
		return false
	end

	if fields[CLEAR_BTN_NAME] then
		data.search = ""
		data.selectedMod = nil
		data.selectedCmdIndex = nil
	elseif fields[SEARCH_BTN_NAME] or fields.key_enter_field == SEARCH_FIELD_NAME then
		data.search = fields[SEARCH_FIELD_NAME] or ""
		data.selectedMod = nil
		data.selectedCmdIndex = nil
	elseif fields[MOD_LIST_NAME] then
		local evt = core.explode_textlist_event(fields[MOD_LIST_NAME])
		if evt.type ~= "CHG" and evt.type ~= "DCL" then
			return false
		end
		data.search = fields[SEARCH_FIELD_NAME] or data.search
		local m = get_filtered_mods(data.search)[evt.index]
		if m then
			data.selectedMod = m
			data.selectedCmdIndex = nil
		end
	elseif fields[CMND_TABLE_NAME] then
		local evt = core.explode_table_event(fields[CMND_TABLE_NAME])
		if evt.type ~= "CHG" and evt.type ~= "DCL" then
			return false
		end
		data.search = fields[SEARCH_FIELD_NAME] or data.search
		local idx = evt.row - 1
		if idx >= 1 then
			data.selectedCmdIndex = idx
			if evt.type == "DCL" then
				local entry = data.selectedMod and get_commands_for_mod(data.selectedMod, data.search)[idx]
				if entry then
					local cmdname, def = entry[1], entry[2]
					local msg = core.colorize(COLOR_CYAN, CMD_MARKER..cmdname)
					if def.params and def.params ~= "" then
						msg = msg.." "..def.params
					end
					if def.description and def.description ~= "" then
						msg = msg..": "..def.description
					end
					if INIT == "client" then
						core.display_chat_message(msg)
					else
						core.chat_send_player(name, msg)
					end
				end
			end
		end
	else
		return false
	end

	return true
end

if INIT == "client" then
	core.register_on_formspec_input(function(formname, fields)
		if formname ~= "__builtin:help_cmds" then
			return
		end
		if handle_help_cmds_fields(nil, fields) then
			core.show_general_help_formspec("")
		end
	end)
else
	core.register_on_player_receive_fields(function(player, formname, fields)
		if formname ~= "__builtin:help_cmds" then
			return
		end
		local name = player:get_player_name()
		if handle_help_cmds_fields(name, fields) then
			core.show_general_help_formspec(name)
		end
	end)
	core.register_on_leaveplayer(function(player)
		formsData[player:get_player_name()] = nil
	end)
end

if INIT ~= "client" then
	function core.show_privs_help_formspec(name)
		core.show_formspec(name, "__builtin:help_privs",
			build_privs_formspec(name))
	end
end
