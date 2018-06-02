--Minetest
--Copyright (C) 2013 sapier
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

--------------------------------------------------------------------------------

local enabled_all = false

local function modname_valid(name)
	return not name:find("[^a-z0-9_]")
end

local function get_formspec(data)
	local mod = data.list:get_list()[data.selected_mod] or {name = ""}

	local retval =
		"size[11.5,7.5,true]" ..
		"label[0.5,0;" .. fgettext("World:") .. "]" ..
		"label[1.75,0;" .. data.worldspec.name .. "]"

	local hard_deps, soft_deps = pkgmgr.get_dependencies(mod.path)
	local hard_deps_str = table.concat(hard_deps, ",")
	local soft_deps_str = table.concat(soft_deps, ",")

	if mod.is_modpack or mod.type == "game" then
		local info = minetest.formspec_escape(
			core.get_content_info(mod.path).description)
		if info == "" then
			if mod.is_modpack then
				info = fgettext("No modpack description provided.")
			else
				info = fgettext("No game description provided.")
			end
		end
		retval = retval ..
			"textarea[0.25,0.7;5.75,7.2;;" .. info .. ";]"
	else
		retval = retval ..
			"label[0,0.7;" .. fgettext("Mod:") .. "]" ..
			"label[0.75,0.7;" .. mod.name .. "]"

		if hard_deps_str == "" then
			if soft_deps_str == "" then
				retval = retval ..
					"label[0,1.25;" ..
					fgettext("This mod does not have dependencies.") .. "]"
			else
				retval = retval ..
					"label[0,1.25;" .. fgettext("No hard dependencies") ..
					"]" ..
					"label[0,1.75;" .. fgettext("Optional dependencies:") ..
					"]" ..
					"textlist[0,2.1;5,4;world_config_optdepends;" ..
					soft_deps_str .. ";0]"
			end
		else
			if soft_deps_str == "" then
				retval = retval ..
					"label[0,1.25;" .. fgettext("Dependencies:") .. "]" ..
					"textlist[0,1.75;5,4;world_config_depends;" ..
					hard_deps_str .. ";0]" ..
					"label[0,6;" .. fgettext("No optional dependencies") .. "]"
			else
				-- set the heights according to the number of list elements
				local ratio = #hard_deps / (#soft_deps + #hard_deps)
				-- avoid making one of the lists too short
				local min_l = math.min(0.5,
					math.min(#soft_deps, #hard_deps) * 0.15)
				ratio = math.max(min_l, math.min(1.0 - min_l, ratio))
				local h1 = 4.0 * ratio
				local h2 = 4.0 - h1
				local s_opt = 1.25 + h1 + 0.5
				retval = retval ..
					"label[0,1.25;" .. fgettext("Dependencies:") .. "]" ..
					"textlist[0,1.75;5," .. h1 .. ";world_config_depends;" ..
					hard_deps_str .. ";0]" ..
					"label[0," .. s_opt .. ";" ..
					fgettext("Optional dependencies:") .. "]" ..
					"textlist[0," .. s_opt + 0.5 .. ";5," .. h2 ..
					";world_config_optdepends;" .. soft_deps_str .. ";0]"
			end
		end
	end
	retval = retval ..
		"button[3.25,7;2.5,0.5;btn_config_world_save;" ..
		fgettext("Save") .. "]" ..
		"button[5.75,7;2.5,0.5;btn_config_world_cancel;" ..
		fgettext("Cancel") .. "]"

	if mod.name ~= "" and not mod.is_game_content then
		if mod.is_modpack then

			if pkgmgr.is_modpack_entirely_enabled(data, mod.name) then
				retval = retval ..
					"button[5.5,0.125;3,0.5;btn_mp_disable;" ..
					fgettext("Disable modpack") .. "]"
			else
				retval = retval ..
					"button[5.5,0.125;3,0.5;btn_mp_enable;" ..
					fgettext("Enable modpack") .. "]"
			end
		else
			retval = retval ..
				"checkbox[5.5,-0.125;cb_mod_enable;" .. fgettext("enabled") ..
				";" .. tostring(mod.enabled) .. "]"
		end
	end
	if enabled_all then
		retval = retval ..
			"button[8.95,0.125;2.5,0.5;btn_disable_all_mods;" ..
			fgettext("Disable all") .. "]"
	else
		retval = retval ..
			"button[8.95,0.125;2.5,0.5;btn_enable_all_mods;" ..
			fgettext("Enable all") .. "]"
	end
	return retval ..
		"tablecolumns[color;tree;text]" ..
		"table[5.5,0.75;5.75,6;world_config_modlist;" ..
		pkgmgr.render_packagelist(data.list) .. ";" .. data.selected_mod .."]"
end

local function handle_buttons(this, fields)
	if fields.world_config_modlist then
		local event = core.explode_table_event(fields.world_config_modlist)
		this.data.selected_mod = event.row
		core.settings:set("world_config_selected_mod", event.row)

		if event.type == "DCL" then
			pkgmgr.enable_mod(this)
		end

		return true
	end

	if fields.key_enter then
		pkgmgr.enable_mod(this)
		return true
	end

	if fields.cb_mod_enable ~= nil then
		pkgmgr.enable_mod(this, core.is_yes(fields.cb_mod_enable))
		return true
	end

	if fields.btn_mp_enable ~= nil or
			fields.btn_mp_disable then
		pkgmgr.enable_mod(this, fields.btn_mp_enable ~= nil)
		return true
	end

	if fields.btn_config_world_save then
		local filename = this.data.worldspec.path .. DIR_DELIM .. "world.mt"

		local worldfile = Settings(filename)
		local mods = worldfile:to_table()

		local rawlist = this.data.list:get_raw_list()

		for i = 1, #rawlist do
			local mod = rawlist[i]
			if not mod.is_modpack and
					not mod.is_game_content then
				if modname_valid(mod.name) then
					worldfile:set("load_mod_" .. mod.name,
						mod.enabled and "true" or "false")
				elseif mod.enabled then
					gamedata.errormessage = fgettext_ne("Failed to enable mo" ..
							"d \"$1\" as it contains disallowed characters. " ..
							"Only characters [a-z0-9_] are allowed.",
							mod.name)
				end
				mods["load_mod_" .. mod.name] = nil
			end
		end

		-- Remove mods that are not present anymore
		for key in pairs(mods) do
			if key:sub(1, 9) == "load_mod_" then
				worldfile:remove(key)
			end
		end

		if not worldfile:write() then
			core.log("error", "Failed to write world config file")
		end

		this:delete()
		return true
	end

	if fields.btn_config_world_cancel then
		this:delete()
		return true
	end

	if fields.btn_enable_all_mods then
		local list = this.data.list:get_raw_list()

		for i = 1, #list do
			if not list[i].is_game_content
					and not list[i].is_modpack then
				list[i].enabled = true
			end
		end
		enabled_all = true
		return true
	end

	if fields.btn_disable_all_mods then
		local list = this.data.list:get_raw_list()

		for i = 1, #list do
			if not list[i].is_game_content
					and not list[i].is_modpack then
				list[i].enabled = false
			end
		end
		enabled_all = false
		return true
	end

	return false
end

function create_configure_world_dlg(worldidx)
	local dlg = dialog_create("sp_config_world", get_formspec, handle_buttons)

	dlg.data.selected_mod = tonumber(
			core.settings:get("world_config_selected_mod")) or 0

	dlg.data.worldspec = core.get_worlds()[worldidx]
	if not dlg.data.worldspec then
		dlg:delete()
		return
	end

	dlg.data.worldconfig = pkgmgr.get_worldconfig(dlg.data.worldspec.path)

	if not dlg.data.worldconfig or not dlg.data.worldconfig.id or
			dlg.data.worldconfig.id == "" then
		dlg:delete()
		return
	end

	dlg.data.list = filterlist.create(
		pkgmgr.preparemodlist,
		pkgmgr.comparemod,
		function(element, uid)
			if element.name == uid then
				return true
			end
		end,
		function(element, criteria)
			if criteria.hide_game and
					element.is_game_content then
				return false
			end

			if criteria.hide_modpackcontents and
					element.modpack ~= nil then
				return false
			end
			return true
		end,
		{
			worldpath = dlg.data.worldspec.path,
			gameid = dlg.data.worldspec.gameid
		}
	)


	if dlg.data.selected_mod > dlg.data.list:size() then
		dlg.data.selected_mod = 0
	end

	dlg.data.list:set_filtercriteria({
		hide_game = dlg.data.hide_gamemods,
		hide_modpackcontents = dlg.data.hide_modpackcontents
	})
	dlg.data.list:add_sort_mechanism("alphabetic", sort_mod_list)
	dlg.data.list:set_sortmode("alphabetic")

	return dlg
end
