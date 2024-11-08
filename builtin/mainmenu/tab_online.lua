--Luanti
--Copyright (C) 2014 sapier
--
--This program is free software; you can redistribute it and/or modify
--it under the terms of the GNU Lesser General Public License as published by
--the Free Software Foundation; either version 2.1 of the License, or
--(at your option) any later version.
--
--This program is distributed in the hope that it will be useful,
--but WITHOUT ANY WARRANTY; without even the implied warranty of
--MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--GNU Lesser General Public License for more details.
--
--You should have received a copy of the GNU Lesser General Public License along
--with this program; if not, write to the Free Software Foundation, Inc.,
--51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

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

	if tabdata.selected then
		if gamedata.fav then
			retval = retval .. "tooltip[btn_delete_favorite;" .. fgettext("Remove favorite") .. "]"
			retval = retval .. "style[btn_delete_favorite;padding=6]"
			retval = retval .. "image_button[5,1.3;0.5,0.5;" .. core.formspec_escape(defaulttexturedir ..
				"server_favorite_delete.png") .. ";btn_delete_favorite;]"
		end
		if gamedata.serverdescription then
			retval = retval .. "textarea[0.25,1.85;5.25,2.7;;;" ..
				core.formspec_escape(gamedata.serverdescription) .. "]"
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

	if tabdata.selected then
		retval = retval .. ";" .. tabdata.selected .. "]"
	else
		retval = retval .. ";0]"
	end

	return retval
end

--------------------------------------------------------------------------------

local function search_server_list(input)
	menudata.search_result = nil
	if #serverlistmgr.servers < 2 then
		return
	end

	-- setup the keyword list
	local keywords = {}
	for word in input:gmatch("%S+") do
		table.insert(keywords, word:lower())
	end

	if #keywords == 0 then
		return
	end

	menudata.search_result = {}

	-- Search the serverlist
	local search_result = {}
	for i, server in ipairs(serverlistmgr.servers) do
		local name_matches, description_matches = true, true
		for _, keyword in ipairs(keywords) do
			name_matches = name_matches and not not
					(server.name or ""):lower():find(keyword, 1, true)
			description_matches = description_matches and not not
					(server.description or ""):lower():find(keyword, 1, true)
		end
		if name_matches or description_matches then
			server.points = #serverlistmgr.servers - i
					+ (name_matches and 50 or 0)
			table.insert(search_result, server)
		end
	end

	if #search_result == 0 then
		return
	end

	table.sort(search_result, function(a, b)
		return a.points > b.points
	end)
	menudata.search_result = search_result
end

local function set_selected_server(tabdata, idx, server)
	-- reset selection
	if idx == nil or server == nil then
		tabdata.selected = nil

		core.settings:set("address", "")
		core.settings:set("remote_port", "30000")
		return
	end

	local address = server.address
	local port    = server.port
	gamedata.serverdescription = server.description

	gamedata.fav = false
	for _, fav in ipairs(serverlistmgr.get_favorites()) do
		if address == fav.address and port == fav.port then
			gamedata.fav = true
			break
		end
	end

	if address and port then
		core.settings:set("address", address)
		core.settings:set("remote_port", port)
	end
	tabdata.selected = idx
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
					core.settings:set("address", gamedata.address)
					core.settings:set("remote_port", gamedata.port)
					core.start()
				end
				return true
			end
			if event.type == "CHG" then
				set_selected_server(tabdata, event.row, server)
				return true
			end
		end
	end

	if fields.btn_delete_favorite then
		local idx = core.get_table_index("servers")
		if not idx then return end
		local server = tabdata.lookup[idx]
		if not server then return end

		serverlistmgr.delete_favorite(server)
		-- the server at [idx+1] will be at idx once list is refreshed
		set_selected_server(tabdata, idx, tabdata.lookup[idx+1])
		return true
	end

	if fields.btn_mp_clear then
		tabdata.search_for = ""
		menudata.search_result = nil
		return true
	end

	if fields.btn_mp_search or fields.key_enter_field == "te_search" then
		tabdata.search_for = fields.te_search
		search_server_list(fields.te_search:lower())
		if menudata.search_result then
			-- first server in row 2 due to header
			set_selected_server(tabdata, 2, menudata.search_result[1])
		end

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

		set_selected_server(tabdata)

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
