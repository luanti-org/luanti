-- Luanti
-- Copyright (C) 2014 sapier
-- SPDX-License-Identifier: LGPL-2.1-or-later

local function get_sorted_servers()
	local servers = {
		fav = {},
		public = {},
		incompatible = {}
	}

	local favs = serverlistmgr.get_favorites()
	local taken_favs = {}
	local result = menudata.search_result or serverlistmgr.servers
	for _, server in ipairs(result) do
		server.is_favorite = false
		for index, fav in ipairs(favs) do
			if server.address == fav.address and server.port == fav.port then
				taken_favs[index] = true
				server.is_favorite = true
				break
			end
		end
		server.is_compatible = is_server_protocol_compat(server.proto_min, server.proto_max)
		if server.is_favorite then
			table.insert(servers.fav, server)
		elseif server.is_compatible then
			table.insert(servers.public, server)
		else
			table.insert(servers.incompatible, server)
		end
	end

	if not menudata.search_result then
		for index, fav in ipairs(favs) do
			if not taken_favs[index] then
				table.insert(servers.fav, fav)
			end
		end
	end

	return servers
end

local function is_selected_fav(server)
	local address = core.settings:get("address")
	local port = tonumber(core.settings:get("remote_port"))

	for _, fav in ipairs(serverlistmgr.get_favorites()) do
		if address == fav.address and port == fav.port then
			return true
		end
	end
	return false
end

-- Persists the selected server in the "address" and "remote_port" settings

local function set_selected_server(server)
	if server == nil then -- reset selection
		core.settings:remove("address")
		core.settings:remove("remote_port")
		return
	end
	local address = server.address
	local port    = server.port
	gamedata.serverdescription = server.description

	if address and port then
		core.settings:set("address", address)
		core.settings:set("remote_port", port)
	end
end

local function find_selected_server()
	local address = core.settings:get("address")
	local port = tonumber(core.settings:get("remote_port"))
	for _, server in ipairs(serverlistmgr.servers) do
		if server.address == address and server.port == port then
			return server
		end
	end
	for _, server in ipairs(serverlistmgr.get_favorites()) do
		if server.address == address and server.port == port then
			return server
		end
	end
end

local function get_formspec(tabview, name, tabdata)
	-- Update the cached supported proto info,
	-- it may have changed after a change by the settings menu.
	common_update_cached_supp_proto()

	if not tabdata.search_for then
		tabdata.search_for = ""
	end

	local retval =
		-- Search
		"field[0.25,0.25;7,0.75;te_search;;" .. core.formspec_escape(tabdata.search_for) .. "]" ..
		"tooltip[te_search;" .. fgettext("Possible filters\ngame:<name>\nmod:<name>\nplayer:<name>") .. "]" ..
		"field_enter_after_edit[te_search;true]" ..
		"container[7.25,0.25]" ..
		"image_button[0,0;0.75,0.75;" .. core.formspec_escape(defaulttexturedir .. "search.png") .. ";btn_mp_search;]" ..
		"image_button[0.75,0;0.75,0.75;" .. core.formspec_escape(defaulttexturedir .. "clear.png") .. ";btn_mp_clear;]" ..
		"image_button[1.5,0;0.75,0.75;" .. core.formspec_escape(defaulttexturedir .. "refresh.png") .. ";btn_mp_refresh;]" ..
		"tooltip[btn_mp_clear;" .. fgettext("Clear") .. "]" ..
		"tooltip[btn_mp_search;" .. fgettext("Search") .. "]" ..
		"tooltip[btn_mp_refresh;" .. fgettext("Refresh") .. "]" ..
		"container_end[]" ..

		"container[9.75,0]" ..
		"box[0,0;5.75,7.1;#666666]" ..

		-- Address / Port
		"label[0.25,0.35;" .. fgettext("Address") .. "]" ..
		"label[4.25,0.35;" .. fgettext("Port") .. "]" ..
		"field[0.25,0.5;4,0.75;te_address;;" ..
			core.formspec_escape(core.settings:get("address")) .. "]" ..
		"field[4.25,0.5;1.25,0.75;te_port;;" ..
			core.formspec_escape(core.settings:get("remote_port")) .. "]" ..

		-- Description Background
		"label[0.25,1.6;" .. fgettext("Server Description") .. "]" ..
		"box[0.25,1.85;5.25,2.7;#999999]"..

		-- Name / Password
		"container[0,4.8]" ..
		"label[0.25,0;" .. fgettext("Name") .. "]" ..
		"label[2.875,0;" .. fgettext("Password") .. "]" ..
		"field[0.25,0.2;2.625,0.75;te_name;;" .. core.formspec_escape(core.settings:get("name")) .. "]" ..
		"pwdfield[2.875,0.2;2.625,0.75;te_pwd;]" ..
		"container_end[]" ..

		-- Connect
		"button[3,6;2.5,0.75;btn_mp_login;" .. fgettext("Login") .. "]"

	if core.settings:get_bool("enable_split_login_register") then
		retval = retval .. "button[0.25,6;2.5,0.75;btn_mp_register;" .. fgettext("Register") .. "]"
	end

	local selected_server = find_selected_server()

	if selected_server then
		if gamedata.serverdescription then
			retval = retval .. "textarea[0.25,1.85;5.25,2.7;;;" ..
				core.formspec_escape(gamedata.serverdescription) .. "]"
		end

		-- Mods button
		local mods = selected_server.mods
		if mods and #mods > 0 then
			local tooltip = ""
			if selected_server.gameid then
				tooltip = fgettext("Game: $1", selected_server.gameid) .. "\n"
			end
			tooltip = tooltip .. fgettext("Number of mods: $1", #mods)

			retval = retval ..
				"tooltip[btn_view_mods;" .. tooltip .. "]" ..
				"style[btn_view_mods;padding=6]" ..
				"image_button[4,1.3;0.5,0.5;" .. core.formspec_escape(defaulttexturedir ..
				"server_view_mods.png") .. ";btn_view_mods;]"
		else
			retval = retval .. "image[4.1,1.4;0.3,0.3;" .. core.formspec_escape(defaulttexturedir ..
				"server_view_mods_unavailable.png") .. "]"
		end

		-- Clients list button
		local clients_list = selected_server.clients_list
		local can_view_clients_list = clients_list and #clients_list > 0
		if can_view_clients_list then
			table.sort(clients_list, function(a, b)
				return a:lower() < b:lower()
			end)
			local max_clients = 5
			if #clients_list > max_clients then
				retval = retval .. "tooltip[btn_view_clients;" ..
						fgettext("Players:\n$1", table.concat(clients_list, "\n", 1, max_clients)) .. "\n..." .. "]"
			else
				retval = retval .. "tooltip[btn_view_clients;" ..
						fgettext("Players:\n$1", table.concat(clients_list, "\n")) .. "]"
			end
			retval = retval .. "style[btn_view_clients;padding=6]"
			retval = retval .. "image_button[4.5,1.3;0.5,0.5;" .. core.formspec_escape(defaulttexturedir ..
				"server_view_clients.png") .. ";btn_view_clients;]"
		else
			retval = retval .. "image[4.6,1.4;0.3,0.3;" .. core.formspec_escape(defaulttexturedir ..
				"server_view_clients_unavailable.png") .. "]"
		end

		-- URL button
		if selected_server.url then
			retval = retval .. "tooltip[btn_server_url;" .. fgettext("Open server website") .. "]"
			retval = retval .. "style[btn_server_url;padding=6]"
			retval = retval .. "image_button[3.5,1.3;0.5,0.5;" ..
				core.formspec_escape(defaulttexturedir .. "server_url.png") .. ";btn_server_url;]"
		else
			retval = retval .. "image[3.6,1.4;0.3,0.3;" .. core.formspec_escape(defaulttexturedir ..
				"server_url_unavailable.png") .. "]"
		end

		-- Favorites toggle button
		if is_selected_fav() then
			retval = retval .. "tooltip[btn_delete_favorite;" .. fgettext("Remove favorite") .. "]"
			retval = retval .. "style[btn_delete_favorite;padding=6]"
			retval = retval .. "image_button[5,1.3;0.5,0.5;" ..
				core.formspec_escape(defaulttexturedir .. "server_favorite_delete.png") .. ";btn_delete_favorite;]"
		else
			retval = retval .. "tooltip[btn_add_favorite;" .. fgettext("Add favorite") .. "]"
			retval = retval .. "style[btn_add_favorite;padding=6]"
			retval = retval .. "image_button[5,1.3;0.5,0.5;" ..
				core.formspec_escape(defaulttexturedir .. "server_favorite.png") .. ";btn_add_favorite;]"
		end
	end

	retval = retval .. "container_end[]"

	-- Table
	retval = retval .. "tablecolumns[" ..
		"image,tooltip=" .. fgettext("Ping") .. "," ..
		"0=" .. core.formspec_escape(defaulttexturedir .. "blank.png") .. "," ..
		"1=" .. core.formspec_escape(defaulttexturedir .. "server_ping_4.png") .. "," ..
		"2=" .. core.formspec_escape(defaulttexturedir .. "server_ping_3.png") .. "," ..
		"3=" .. core.formspec_escape(defaulttexturedir .. "server_ping_2.png") .. "," ..
		"4=" .. core.formspec_escape(defaulttexturedir .. "server_ping_1.png") .. "," ..
		"5=" .. core.formspec_escape(defaulttexturedir .. "server_favorite.png") .. "," ..
		"6=" .. core.formspec_escape(defaulttexturedir .. "server_public.png") .. "," ..
		"7=" .. core.formspec_escape(defaulttexturedir .. "server_incompatible.png") .. ";" ..
		"color,span=1;" ..
		"text,align=inline;"..
		"color,span=1;" ..
		"text,align=inline,width=4.25;" ..
		"image,tooltip=" .. fgettext("Creative mode") .. "," ..
		"0=" .. core.formspec_escape(defaulttexturedir .. "blank.png") .. "," ..
		"1=" .. core.formspec_escape(defaulttexturedir .. "server_flags_creative.png") .. "," ..
		"align=inline,padding=0.25,width=1.5;" ..
		--~ PvP = Player versus Player
		"image,tooltip=" .. fgettext("Damage / PvP") .. "," ..
		"0=" .. core.formspec_escape(defaulttexturedir .. "blank.png") .. "," ..
		"1=" .. core.formspec_escape(defaulttexturedir .. "server_flags_damage.png") .. "," ..
		"2=" .. core.formspec_escape(defaulttexturedir .. "server_flags_pvp.png") .. "," ..
		"align=inline,padding=0.25,width=1.5;" ..
		"color,align=inline,span=1;" ..
		"text,align=inline,padding=1]" ..
		"table[0.25,1;9.25,5.8;servers;"

	local servers = get_sorted_servers()

	local dividers = {
		fav = "5,#ffff00," .. fgettext("Favorites") .. ",,,0,0,,",
		public = "6,#4bdd42," .. fgettext("Public Servers") .. ",,,0,0,,",
		incompatible = "7,"..mt_color_grey.."," .. fgettext("Incompatible Servers") .. ",,,0,0,,"
	}
	local order = {"fav", "public", "incompatible"}

	tabdata.lookup = {} -- maps row number to server
	local rows = {}
	for _, section in ipairs(order) do
		local section_servers = servers[section]
		if next(section_servers) ~= nil then
			rows[#rows + 1] = dividers[section]
			for _, server in ipairs(section_servers) do
				tabdata.lookup[#rows + 1] = server
				rows[#rows + 1] = render_serverlist_row(server)
			end
		end
	end

	retval = retval .. table.concat(rows, ",")

	local selected_row_idx = 0
	if selected_server then
		for i, server in pairs(tabdata.lookup) do
			if selected_server.address == server.address and
					selected_server.port == server.port then
				selected_row_idx = i
				break
			end
		end
	end
	retval = retval .. ";" .. selected_row_idx .. "]"

	return retval
end

--------------------------------------------------------------------------------

local function parse_search_input(input)
	if not input:find("%S") then
		return -- Return nil if nothing to search for
	end

	-- Search is not case sensitive
	input = input:lower()

	local query = {keywords = {}, mods = {}, players = {}}

	-- Process quotation enclosed parts
	input = input:gsub('(%S?)"([^"]*)"(%S?)', function(before, match, after)
		if before == "" and after == "" then -- Also have be separated by spaces
			table.insert(query.keywords, match)
			return " "
		end
		return before..'"'..match..'"'..after
	end)

	-- Separate by space characters and handle special prefixes
	-- (words with special prefixes need an exact match and none of them can contain spaces)
	for word in input:gmatch("%S+") do
		local mod = word:match("^mod:(.*)")
		table.insert(query.mods, mod)
		local player = word:match("^player:(.*)")
		table.insert(query.players, player)
		local game = word:match("^game:(.*)")
		query.game = query.game or game
		if not (mod or player or game) then
			table.insert(query.keywords, word)
		end
	end

	return query
end

-- Prepares the server to be used for searching
local function uncapitalize_server(server)
	local function table_lower(t)
		local r = {}
		for i, s in ipairs(t or {}) do
			r[i] = s:lower()
		end
		return r
	end

	return {
		name = (server.name or ""):lower(),
		description = (server.description or ""):lower(),
		gameid = (server.gameid or ""):lower(),
		mods = table_lower(server.mods),
		clients_list = table_lower(server.clients_list),
	}
end

-- Returns false if the query does not match
-- otherwise returns a number to adjust the sorting priority
local function matches_query(server, query)
	-- Search is not case sensitive
	server = uncapitalize_server(server)

	-- Check if mods found
	for _, mod in ipairs(query.mods) do
		if table.indexof(server.mods, mod) < 0 then
			return false
		end
	end

	-- Check if players found
	for _, player in ipairs(query.players) do
		if table.indexof(server.clients_list, player) < 0 then
			return false
		end
	end

	-- Check if game matches
	if query.game and query.game ~= server.gameid then
		return false
	end

	-- Check if keyword found
	local name_matches = true
	local description_matches = true
	for _, keyword in ipairs(query.keywords) do
		name_matches = name_matches and server.name:find(keyword, 1, true)
		description_matches = description_matches and server.description:find(keyword, 1, true)
	end

	return name_matches and 50 or description_matches and 0
end

local function search_server_list(input, tabdata)
	menudata.search_result = nil
	if #serverlistmgr.servers < 2 then
		return
	end


	tabdata.pre_search_selection = tabdata.pre_search_selection or find_selected_server()

	-- setup the search query
	local query = parse_search_input(input)
	if not query then
		return
	end

	menudata.search_result = {}

	-- Search the serverlist
	local search_result = {}
	for i, server in ipairs(serverlistmgr.servers) do
		local match = matches_query(server, query)
		if match then
			server.points = #serverlistmgr.servers - i + match
			table.insert(search_result, server)
		end
	end

	if #search_result == 0 then
		return
	end

	local current_server = find_selected_server()

	table.sort(search_result, function(a, b)
		return a.points > b.points
	end)
	menudata.search_result = search_result

	-- Keep current selection if it's in search results
	if current_server then
	    for _, server in ipairs(search_result) do
			if server.address == current_server.address and
					server.port == current_server.port then
				return
			end
		end
	end

	-- Find first compatible server (favorite or public)
	for _, server in ipairs(search_result) do
		if is_server_protocol_compat(server.proto_min, server.proto_max) then
			set_selected_server(server)
			return
		end
	end
	-- If no compatible server found, clear selection
	set_selected_server(nil)
end

local function main_button_handler(tabview, fields, name, tabdata)
	if fields.te_name then
		gamedata.playername = fields.te_name
		core.settings:set("name", fields.te_name)
	end

	if fields.servers then
		local event = core.explode_table_event(fields.servers)
		local server = tabdata.lookup[event.row]

		if server then
			if event.type == "DCL" then
				if not is_server_protocol_compat_or_error(
							server.proto_min, server.proto_max) then
					return true
				end

				gamedata.address    = server.address
				gamedata.port       = server.port
				gamedata.playername = fields.te_name
				gamedata.selected_world = 0

				if fields.te_pwd then
					gamedata.password = fields.te_pwd
				end

				gamedata.servername        = server.name
				gamedata.serverdescription = server.description

				if gamedata.address and gamedata.port then
					set_selected_server(server)
					core.start()
				end
				return true
			end
			if event.type == "CHG" then
				set_selected_server(server)
				tabdata.pre_search_selection = nil
				return true
			end
		end
	end

	if fields.btn_add_favorite then
		serverlistmgr.add_favorite(find_selected_server())
		return true
	end

	if fields.btn_delete_favorite then
		local idx = core.get_table_index("servers")
		if not idx then return end

		serverlistmgr.delete_favorite(tabdata.lookup[idx])
		set_selected_server(tabdata.lookup[idx+1])
		return true
	end

	if fields.btn_server_url then
		core.open_url_dialog(find_selected_server().url)
		return true
	end

	if fields.btn_view_clients then
		local dlg = create_clientslist_dialog(find_selected_server())
		dlg:set_parent(tabview)
		tabview:hide()
		dlg:show()
		return true
	end

	if fields.btn_view_mods then
		local dlg = create_server_list_mods_dialog(find_selected_server())
		dlg:set_parent(tabview)
		tabview:hide()
		dlg:show()
		return true
	end

	if fields.btn_mp_clear then
		tabdata.search_for = ""
		menudata.search_result = nil
		if tabdata.pre_search_selection then
			set_selected_server(tabdata.pre_search_selection)
			tabdata.pre_search_selection = nil
		end
		return true
	end

	if fields.btn_mp_search or fields.key_enter_field == "te_search" then
		tabdata.search_for = fields.te_search
		search_server_list(fields.te_search, tabdata)
		return true
	end

	if fields.btn_mp_refresh then
		serverlistmgr.sync()
		return true
	end

	local host_filled = (fields.te_address ~= "") and fields.te_port:match("^%s*[1-9][0-9]*%s*$")
	local te_port_number = tonumber(fields.te_port)

	if (fields.btn_mp_login or fields.key_enter) and host_filled then
		gamedata.playername = fields.te_name
		gamedata.password   = fields.te_pwd
		gamedata.address    = fields.te_address
		gamedata.port       = te_port_number

		local enable_split_login_register = core.settings:get_bool("enable_split_login_register")
		gamedata.allow_login_or_register = enable_split_login_register and "login" or "any"
		gamedata.selected_world = 0

		local idx = core.get_table_index("servers")
		local server = idx and tabdata.lookup[idx]

		if server and server.address == gamedata.address and
				server.port == gamedata.port then

			serverlistmgr.add_favorite(server)

			gamedata.servername        = server.name
			gamedata.serverdescription = server.description

			if not is_server_protocol_compat_or_error(
						server.proto_min, server.proto_max) then
				return true
			end
		else
			gamedata.servername        = ""
			gamedata.serverdescription = ""

			serverlistmgr.add_favorite({
				address = gamedata.address,
				port = gamedata.port,
			})
		end

		core.settings:set("address",     gamedata.address)
		core.settings:set("remote_port", gamedata.port)

		core.start()
		return true
	end

	if fields.btn_mp_register and host_filled then
		local idx = core.get_table_index("servers")
		local server = idx and tabdata.lookup[idx]
		if server and (server.address ~= fields.te_address or server.port ~= te_port_number) then
			server = nil
		end

		if server and not is_server_protocol_compat_or_error(
					server.proto_min, server.proto_max) then
			return true
		end

		local dlg = create_register_dialog(fields.te_address, te_port_number, server)
		dlg:set_parent(tabview)
		tabview:hide()
		dlg:show()
		return true
	end

	return false
end

local function on_change(type)
	if type == "ENTER" then
		mm_game_theme.set_engine()
		serverlistmgr.sync()
	end
end

return {
	name = "online",
	caption = fgettext("Join Game"),
	cbf_formspec = get_formspec,
	cbf_button_handler = main_button_handler,
	on_change = on_change
}
