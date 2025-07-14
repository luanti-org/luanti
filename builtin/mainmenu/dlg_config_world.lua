-- Luanti
-- Copyright (C) 2013 sapier
-- SPDX-License-Identifier: LGPL-2.1-or-later

--------------------------------------------------------------------------------

local enabled_all = false

local function modname_valid(name)
	return not name:find("[^a-z0-9_]")
end

local function init_data(data)
	data.search_for = data.search_for or ""
	data.list = filterlist.create(
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

			-- Filter by search text if provided
			if criteria.search_text and criteria.search_text ~= "" then
				local search_text = criteria.search_text:lower()
				local name = element.name:lower()
				if not name:find(search_text, 1, true) then
					return false
				end
			end

			return true
		end,
		{
			worldpath = data.worldspec.path,
			gameid = data.worldspec.gameid
		})

	if data.selected_mod > data.list:size() then
		data.selected_mod = 0
	end

	data.list:set_filtercriteria({
		hide_game = data.hide_gamemods,
		hide_modpackcontents = data.hide_modpackcontents,
		search_text = data.search_for
	})
	-- Sorting is already done by pgkmgr.get_mods
end


-- Returns errors errors and a list of all enabled mods (inc. game and world mods)
--
-- `with_errors` is a table from mod virtual path to `{ type = "error" | "warning" }`.
-- `enabled_mods_by_name` is a table from mod virtual path to `true`.
--
-- @param world_path Path to the world
-- @param all_mods List of mods, with `enabled` property.
-- @returns with_errors, enabled_mods_by_name
local function check_mod_configuration(world_path, all_mods)
	-- Build up lookup tables for enabled mods and all mods by vpath
	local enabled_mod_paths = {}
	local all_mods_by_vpath = {}
	for _, mod in ipairs(all_mods)  do
		if mod.type == "mod" then
			all_mods_by_vpath[mod.virtual_path] = mod
		end
		if mod.enabled then
			enabled_mod_paths[mod.virtual_path] = mod.path
		end
	end

	-- Use the engine's mod configuration code to resolve dependencies and return any errors
	local config_status = core.check_mod_configuration(world_path, enabled_mod_paths)

	-- Build the list of enabled mod virtual paths
	local enabled_mods_by_name = {}
	for _, mod in ipairs(config_status.satisfied_mods) do
		assert(mod.virtual_path ~= "")
		enabled_mods_by_name[mod.name] = all_mods_by_vpath[mod.virtual_path] or mod
	end
	for _, mod in ipairs(config_status.unsatisfied_mods) do
		assert(mod.virtual_path ~= "")
		enabled_mods_by_name[mod.name] = all_mods_by_vpath[mod.virtual_path] or mod
	end

	-- Build the table of errors
	local with_error = {}
	for _, mod in ipairs(config_status.unsatisfied_mods) do
		local error = { type = "warning" }
		with_error[mod.virtual_path] = error

		for _, depname in ipairs(mod.unsatisfied_depends) do
			if not enabled_mods_by_name[depname] then
				error.type = "error"
				break
			end
		end
	end

	return with_error, enabled_mods_by_name
end

local function get_formspec(data)
	if not data.list then
		init_data(data)
	end

	local all_mods = data.list:get_list()
	local with_error, enabled_mods_by_name = check_mod_configuration(data.worldspec.path, all_mods)

	local mod = all_mods[data.selected_mod] or {name = ""}
	local use_technical_names = core.settings:get_bool("show_technical_names")

	-- Modern layout with consistent spacing and alignment
	local retval = "size[12,8,true]"

	-- Header area with world name
	retval = retval ..
		"label[0.5,0.4;" .. fgettext("World:") .. " " .. core.formspec_escape(data.worldspec.name) .. "]"

	-- Search box with better positioning
	retval = retval ..
		"field[8.0,0.3;3,0.8;te_search;;" .. core.formspec_escape(data.search_for) .. "]" ..
		"field_enter_after_edit[te_search;true]" ..
		"image_button[11.0,0.15;0.8,0.8;" .. core.formspec_escape(defaulttexturedir .. "search.png") .. ";btn_mod_search;]" ..
		"tooltip[btn_mod_search;" .. fgettext("Search") .. "]" ..
		"image_button[11.0,1.0;0.8,0.8;" .. core.formspec_escape(defaulttexturedir .. "clear.png") .. ";btn_mod_clear;]" ..
		"tooltip[btn_mod_clear;" .. fgettext("Clear") .. "]"

	-- Left panel: Mod list with header
	retval = retval ..
		"label[0.5,1.0;" .. fgettext("Available Mods:") .. "]" ..
		"tablecolumns[color;tree;image,align=inline,width=1.5,0=" .. core.formspec_escape(defaulttexturedir .. "blank.png") ..
			",1=" .. core.formspec_escape(defaulttexturedir .. "checkbox_16.png") ..
			",2=" .. core.formspec_escape(defaulttexturedir .. "error_icon_orange.png") ..
			",3=" .. core.formspec_escape(defaulttexturedir .. "error_icon_red.png") .. ";text]" ..
		"table[0.5,1.5;5.5,5.5;world_config_modlist;" ..
		pkgmgr.render_packagelist(data.list, use_technical_names, with_error) .. ";" .. data.selected_mod .."]"

	-- Find More Mods button positioned under the mod list
	if not mod.is_modpack and mod.type ~= "game" then
		retval = retval ..
			"button[0.5,7.1;2.5,0.7;btn_config_world_cdb;" ..
			fgettext("Find More Mods") .. "]"
	end

	-- Enable/Disable All button
	if enabled_all then
		retval = retval ..
			"button[3.5,7.1;2.5,0.7;btn_disable_all_mods;" ..
			fgettext("Disable all") .. "]"
	else
		retval = retval ..
			"button[3.5,7.1;2.5,0.7;btn_enable_all_mods;" ..
			fgettext("Enable all") .. "]"
	end

	-- Right panel: Mod details with header
	retval = retval ..
		"label[6.5,1.0;" .. fgettext("Mod Details:") .. "]"

	-- Mod details content
	if mod.is_modpack or mod.type == "game" then
		local info = core.formspec_escape(
			core.get_content_info(mod.path).description)
		if info == "" then
			if mod.is_modpack then
				info = fgettext("No modpack description provided.")
			else
				info = fgettext("No game description provided.")
			end
		end
		retval = retval ..
			"label[6.5,1.5;" .. fgettext("Name:") .. " " .. mod.name .. "]" ..
			"textarea[6.5,2.0;5.0,5.0;;" .. info .. ";]"
	else
		local hard_deps, soft_deps = pkgmgr.get_dependencies(mod.path)

		-- Add error messages to dep lists
		if mod.enabled or mod.is_game_content then
			for i, dep_name in ipairs(hard_deps) do
				local dep = enabled_mods_by_name[dep_name]
				if not dep then
					hard_deps[i] = mt_color_red .. dep_name .. " " .. fgettext("(Unsatisfied)")
				elseif with_error[dep.virtual_path] then
					hard_deps[i] = mt_color_orange .. dep_name .. " " .. fgettext("(Enabled, has error)")
				else
					hard_deps[i] = mt_color_green .. dep_name
				end
			end
			for i, dep_name in ipairs(soft_deps) do
				local dep = enabled_mods_by_name[dep_name]
				if dep and with_error[dep.virtual_path] then
					soft_deps[i] = mt_color_orange .. dep_name .. " " .. fgettext("(Enabled, has error)")
				elseif dep then
					soft_deps[i] = mt_color_green .. dep_name
				end
			end
		end

		local hard_deps_str = table.concat(hard_deps, ",")
		local soft_deps_str = table.concat(soft_deps, ",")

		retval = retval ..
			"label[6.5,1.5;" .. fgettext("Name:") .. " " .. mod.name .. "]"

		if hard_deps_str == "" and soft_deps_str == "" then
			retval = retval ..
				"label[6.5,2.0;" .. fgettext("No dependencies") .. "]"
		else
			-- Start dependencies section at a fixed position
			local y_pos = 2.0

			if hard_deps_str ~= "" then
				retval = retval ..
					"label[6.5," .. y_pos .. ";" .. fgettext("Dependencies:") .. "]" ..
					"textlist[6.5," .. (y_pos + 0.5) .. ";5,1.5;world_config_depends;" ..
					hard_deps_str .. ";0]"
				y_pos = y_pos + 2.0  -- Reduced spacing between dependency sections
			end

			if soft_deps_str ~= "" then
				retval = retval ..
					"label[6.5," .. y_pos .. ";" .. fgettext("Optional dependencies:") .. "]" ..
					"textlist[6.5," .. (y_pos + 0.5) .. ";5,1.5;world_config_optdepends;" ..
					soft_deps_str .. ";0]"
			end
		end
	end

	-- Mod/Modpack enable/disable controls
	if mod.name ~= "" and not mod.is_game_content then
		if mod.is_modpack then
			if pkgmgr.is_modpack_entirely_enabled(data, mod.name) then
				retval = retval ..
					"button[9.0,7.1;2.5,0.7;btn_mp_disable;" ..
					fgettext("Disable modpack") .. "]"
			else
				retval = retval ..
					"button[9.0,7.1;2.5,0.7;btn_mp_enable;" ..
					fgettext("Enable modpack") .. "]"
			end
		else
			retval = retval ..
				"checkbox[6.5,7.1;cb_mod_enable;" .. fgettext("Enable mod") ..
				";" .. tostring(mod.enabled) .. "]"
		end
	end

	-- Bottom buttons - Save and Cancel
	retval = retval ..
		"button[6.5,7.1;2.5,0.7;btn_config_world_save;" ..
		fgettext("Save") .. "]" ..
		"button[9.0,7.1;2.5,0.7;btn_config_world_cancel;" ..
		fgettext("Cancel") .. "]"

	return retval
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

	-- Handle search functionality
	if fields.btn_mod_search or fields.key_enter_field == "te_search" then
		this.data.search_for = fields.te_search
		init_data(this.data)
		return true
	end

	if fields.btn_mod_clear then
		this.data.search_for = ""
		init_data(this.data)
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
		local was_set = {}

		for i = 1, #rawlist do
			local mod = rawlist[i]
			if not mod.is_modpack and
					not mod.is_game_content then
				if modname_valid(mod.name) then
					if mod.enabled then
						worldfile:set("load_mod_" .. mod.name, mod.virtual_path)
						was_set[mod.name] = true
					elseif not was_set[mod.name] then
						worldfile:remove("load_mod_" .. mod.name)
					end
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

	if fields.btn_config_world_cdb then
		this.data.list = nil

		local dlg = create_contentdb_dlg("mod")
		dlg:set_parent(this)
		this:hide()
		dlg:show()
		return true
	end

	if fields.btn_enable_all_mods then
		local list = this.data.list:get_raw_list()

		-- When multiple copies of a mod are installed, we need to avoid enabling multiple of them
		-- at a time. So lets first collect all the enabled mods, and then use this to exclude
		-- multiple enables.

		local was_enabled = {}
		for i = 1, #list do
			if not list[i].is_game_content
					and not list[i].is_modpack and list[i].enabled then
				was_enabled[list[i].name] = true
			end
		end

		for i = 1, #list do
			if not list[i].is_game_content and not list[i].is_modpack and
					not was_enabled[list[i].name] then
				list[i].enabled = true
				was_enabled[list[i].name] = true
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

	return dlg
end
