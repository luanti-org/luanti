--Minetest
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

local server_lookup -- can be used to look up server by row number in GUI

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

	local selected
	if menudata.search_result then
		selected = menudata.search_result[tabdata.selected]
	else
		selected = serverlistmgr.servers[tabdata.selected]
	end

	if not tabdata.search_for then
		tabdata.search_for = ""
	end

	local retval =
		-- Search
		"field[1,0.25;6,0.75;te_search;;" .. core.formspec_escape(tabdata.search_for) .. "]" ..
		"image_button[7,0.25;0.75,0.75;" .. core.formspec_escape(defaulttexturedir .. "search.png") .. ";btn_mp_search;]" ..
		"image_button[0.25,0.25;0.75,0.75;" .. core.formspec_escape(defaulttexturedir .. "clear.png") .. ";btn_mp_clear;]" ..
		"image_button[7.75,0.25;0.75,0.75;" .. core.formspec_escape(defaulttexturedir .. "refresh.png") .. ";btn_mp_refresh;]" ..
		"tooltip[btn_mp_clear;" .. fgettext("Clear") .. "]" ..
		"tooltip[btn_mp_search;" .. fgettext("Search") .. "]" ..
		"tooltip[btn_mp_refresh;" .. fgettext("Refresh") .. "]" ..

		"container[8.75,0]" ..
		"box[0,0;4.75,7;#888888]" ..

		-- Address / Port
		"label[0.25,0.35;" .. fgettext("Address") .. "]" ..
		"label[3.5,0.35;" .. fgettext("Port") .. "]" ..
		"field[0.25,0.5;3.25,0.5;te_address;;" ..
			core.formspec_escape(core.settings:get("address")) .. "]" ..
		"field[3.5,0.5;1,0.5;te_port;;" ..
			core.formspec_escape(core.settings:get("remote_port")) .. "]" ..

		-- Name / Password
		"label[0.25,1.55;" .. fgettext("Name") .. "]" ..
		"label[3,1.55;" .. fgettext("Password") .. "]" ..
		"field[0.25,1.75;2.75,0.5;te_name;;" ..
			core.formspec_escape(core.settings:get("name")) .. "]" ..
		"pwdfield[3,1.75;1.5,0.5;te_pwd;]" ..

		-- Description Background
		"label[0.25,3.05;" .. fgettext("Server Description") .. "]" ..
		"box[0.25,3.25;4.25,2.5;#999999]"..

		-- Connect
		"button[2.5,6;2,0.75;btn_mp_connect;" .. fgettext("Connect") .. "]"

	if tabdata.selected and selected then
		if gamedata.fav then
			retval = retval .. "button[0.25,6;2,0.75;btn_delete_favorite;" ..
				fgettext("Del. Favorite") .. "]"
		end
		if selected.description then
			retval = retval .. "textarea[0.25,3.25;4.25,2.5;;;" ..
				core.formspec_escape(gamedata.serverdescription or "") .. "]"
		end
	end

	retval = retval .. "container_end[]"

	-- Table
	retval = retval .. "tablecolumns[" ..
		"image," ..
		"0=" .. core.formspec_escape(defaulttexturedir .. "blank.png") .. "," ..
		"1=" .. core.formspec_escape(defaulttexturedir .. "server_ping_4.png") .. "," ..
		"2=" .. core.formspec_escape(defaulttexturedir .. "server_ping_3.png") .. "," ..
		"3=" .. core.formspec_escape(defaulttexturedir .. "server_ping_2.png") .. "," ..
		"4=" .. core.formspec_escape(defaulttexturedir .. "server_ping_1.png") .. "," ..
		"5=" .. core.formspec_escape(defaulttexturedir .. "server_favorite.png") .. "," ..
		"6=" .. core.formspec_escape(defaulttexturedir .. "server_divider_discover.png") .. "," ..
		"7=" .. core.formspec_escape(defaulttexturedir .. "server_divider_incompatible.png") .. ";" ..
		"color,span=1;" ..
		"text,align=inline;"..
		"color,span=1;" ..
		"text,align=inline,width=4.25;" ..
		"image,tooltip=" .. fgettext("Creative") .. "," ..
		"0=" .. core.formspec_escape(defaulttexturedir .. "blank.png") .. "," ..
		"1=" .. core.formspec_escape(defaulttexturedir .. "server_flags_creative.png") .. "," ..
		"align=inline,padding=0.25,width=1;" ..
		"image,tooltip=" .. fgettext("Damage") .. "," ..
		"0=" .. core.formspec_escape(defaulttexturedir .. "blank.png") .. "," ..
		"1=" .. core.formspec_escape(defaulttexturedir .. "server_flags_damage.png") .. "," ..
		"2=" .. core.formspec_escape(defaulttexturedir .. "server_flags_pvp.png") .. "," ..
		"align=inline,padding=0.25,width=1.5;" ..
		"color,align=inline,span=1;" ..
		"text,align=inline,padding=1]" ..
		"table[0.25,1;8.25,5.75;servers;"

	local servers = get_sorted_servers()

	local dividers = {
		fav = "5,#ffff00," .. fgettext("Favorite Servers") .. ",,,0,0,,",
		public = "6,"..mt_color_green.."," .. fgettext("Discover Servers") .. ",,,0,0,,",
		incompatible = "7,"..mt_color_grey.."," .. fgettext("Incompatible Servers") .. ",,,0,0,,"
	}
	local order = {"fav", "public", "incompatible"}

	server_lookup = {}
	local rows = {}
	for _, section in ipairs(order) do
		local section_servers = servers[section]
		if next(section_servers) ~= nil then
			server_lookup[#rows + 1] = false
			rows[#rows + 1] = dividers[section]
			for _, server in ipairs(section_servers) do
				server_lookup[#rows + 1] = server
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

	return retval, "size[13.5,7,false]real_coordinates[true]"
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
		word = word:gsub("(%W)", "%%%1")
		table.insert(keywords, word)
	end

	if #keywords == 0 then
		return
	end

	menudata.search_result = {}

	-- Search the serverlist
	local search_result = {}
	for i = 1, #serverlistmgr.servers do
		local server = serverlistmgr.servers[i]
		local found = 0
		for k = 1, #keywords do
			local keyword = keywords[k]
			if server.name then
				local sername = server.name:lower()
				local _, count = sername:gsub(keyword, keyword)
				found = found + count * 4
			end

			if server.description then
				local desc = server.description:lower()
				local _, count = desc:gsub(keyword, keyword)
				found = found + count * 2
			end
		end
		if found > 0 then
			local points = (#serverlistmgr.servers - i) / 5 + found
			server.points = points
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

	local first_server = search_result[1]
	core.settings:set("address",     first_server.address)
	core.settings:set("remote_port", first_server.port)
	gamedata.serverdescription = first_server.description
end

local function main_button_handler(tabview, fields, name, tabdata)
	if fields.te_name then
		gamedata.playername = fields.te_name
		core.settings:set("name", fields.te_name)
	end

	if fields.servers then
		local event = core.explode_table_event(fields.servers)
		local server = server_lookup[event.row]

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
				gamedata.fav = false
				local favs = serverlistmgr.get_favorites()
				local address = server.address
				local port    = server.port
				gamedata.serverdescription = server.description

				for _, fav in ipairs(favs) do
					if server.address == fav.address and
							server.port == fav.port then
						gamedata.fav = true
						break
					end
				end

				if address and port then
					core.settings:set("address", address)
					core.settings:set("remote_port", port)
				end
				tabdata.selected = event.row
				return true
			end
		end
	end

	if fields.btn_delete_favorite then
		local idx = core.get_table_index("servers")
		if not idx then return end

		serverlistmgr.delete_favorite(server_lookup[idx])
		serverlistmgr.sync()
		tabdata.selected = nil

		core.settings:set("address", "")
		core.settings:set("remote_port", "30000")
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

		tabdata.selected = menudata.search_result and 1

		return true
	end

	if fields.btn_mp_refresh then
		serverlistmgr.sync()
		return true
	end

	if (fields.btn_mp_connect or fields.key_enter)
			and fields.te_address ~= "" and fields.te_port then
		gamedata.playername = fields.te_name
		gamedata.password   = fields.te_pwd
		gamedata.address    = fields.te_address
		gamedata.port       = tonumber(fields.te_port)
		gamedata.selected_world = 0

		local idx = core.get_table_index("servers")
		local server = idx and server_lookup[idx]

		if server and server.address == gamedata.address and
				server.port == gamedata.port then

			serverlistmgr.add_favorite(fav)

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

	return false
end

local function on_change(type, old_tab, new_tab)
	if type == "LEAVE" then return end
	serverlistmgr.sync()
end

return {
	name = "online",
	caption = fgettext("Join Game"),
	cbf_formspec = get_formspec,
	cbf_button_handler = main_button_handler,
	on_change = on_change
}
