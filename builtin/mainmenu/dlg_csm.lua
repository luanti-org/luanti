--Luanti
--Copyright (C) 2025 ProunceDev <prouncedev@gmail.com>
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

local packages_raw, packages, tab_selected_pkg

local function modname_valid(name)
	return not name:find("[^a-z0-9_]")
end

local function get_formspec(dlgview, name, tabdata)
	if pkgmgr.clientmods == nil then
		pkgmgr.reload_global_mods()
	end

	if packages == nil then
		packages_raw = {}
		table.insert_all(packages_raw, pkgmgr.clientmods:get_list())

		local function get_data()
			return packages_raw
		end

		local function is_equal(element, uid) --uid match
			return (element.type == "game" and element.id == uid) or
					element.name == uid
		end

		packages = filterlist.create(get_data, pkgmgr.compare_package, is_equal, nil, {})

		local filename = core.get_clientmodpath() .. DIR_DELIM .. "mods.conf"
		local conffile = Settings(filename)
		local mods = conffile:to_table()

		for i = 1, #packages_raw do
			local mod = packages_raw[i]
			if mod.is_clientside then
				if modname_valid(mod.name) then
					conffile:set("load_mod_" .. mod.name,
						mod.enabled and "true" or "false")
				elseif mod.enabled then
					gamedata.errormessage = fgettext_ne("Failed to enable clientmod" ..
						" \"$1\" as it contains disallowed characters. " ..
						"Only characters [a-z0-9_] are allowed.",
						mod.name)
				end
				mods["load_mod_" .. mod.name] = nil
			end
		end

		-- Remove mods that are not present anymore
		for key in pairs(mods) do
			if key:sub(1, 9) == "load_mod_" then
				conffile:remove(key)
			end
		end
		if not conffile:write() then
			core.log("error", "Failed to write clientmod config file")
		end
	end

	if tab_selected_pkg == nil then
		tab_selected_pkg = 1
	end

	local use_technical_names = core.settings:get_bool("show_technical_names")


	local fs = {
		"size[6.5,7]",
		"label[0.25,-0.1;", fgettext("Installed Client Mods"), "]",
		"tablecolumns[color;tree;text]",
		"table[0.15,0.5;6,5.7;pkglist;", pkgmgr.render_packagelist(packages, use_technical_names), ";", tab_selected_pkg, "]",
		"button[0.15,5.5;3,3;back;", fgettext("Back"), "]"
	}


	local selected_pkg
	if filterlist.size(packages) >= tab_selected_pkg then
		selected_pkg = packages:get_list()[tab_selected_pkg]
	end

	if selected_pkg ~= nil then
		if selected_pkg.type == "mod" then
			if selected_pkg.is_clientside then
				if selected_pkg.enabled then
					table.insert_all(fs, {
						"button[3.36,5.5;3,3;btn_csm_mgr_disable_mod;", fgettext("Disable"), "]"
					})
				else
					table.insert_all(fs, {
						"button[3.36,5.5;3,3;btn_csm_mgr_enable_mod;", fgettext("Enable"), "]"
					})
				end
			end
		end
	end
	return table.concat(fs, "")
end

--------------------------------------------------------------------------------
local function handle_doubleclick(pkg, pkg_name)
	pkgmgr.enable_mod({data = {list = packages, selected_mod = pkg_name}})
	packages = nil
end

--------------------------------------------------------------------------------
local function handle_buttons(dlgview, fields, tabname, tabdata)
	if fields["pkglist"] ~= nil then
		local event = core.explode_table_event(fields["pkglist"])
		tab_selected_pkg = event.row
		if event.type == "DCL" then
			handle_doubleclick(packages:get_list()[tab_selected_pkg], tab_selected_pkg)
		end
		return true
	end

	if fields.btn_csm_mgr_mp_enable ~= nil or fields.btn_csm_mgr_mp_disable ~= nil then
		pkgmgr.enable_mod({data = {list = packages, selected_mod = tab_selected_pkg}}, fields.btn_csm_mgr_mp_enable ~= nil)
		packages = nil
		return true
	end

	if fields.btn_csm_mgr_enable_mod ~= nil or fields.btn_csm_mgr_disable_mod ~= nil then
		pkgmgr.enable_mod({data = {list = packages, selected_mod = tab_selected_pkg}}, fields.btn_csm_mgr_enable_mod ~= nil)
		packages = nil
		return true
	end

	if fields.back then
		dlgview:delete()
		return true
	end

	if fields["btn_csm_mgr_rename_modpack"] ~= nil then
		local mod = packages:get_list()[tab_selected_pkg]
		local dlg_renamemp = create_rename_modpack_dlg(mod)
		dlg_renamemp:set_parent(dlgview)
		dlgview:hide()
		dlg_renamemp:show()
		packages = nil
		return true
	end

	if fields["btn_csm_mgr_delete_mod"] ~= nil then
		local mod = packages:get_list()[tab_selected_pkg]
		local dlg_delmod = create_delete_content_dlg(mod)
		dlg_delmod:set_parent(dlgview)
		dlgview:hide()
		dlg_delmod:show()
		packages = nil
		return true
	end

	if fields.btn_csm_mgr_use_txp or fields.btn_csm_mgr_disable_txp then
		local txp_path = ""
		if fields.btn_csm_mgr_use_txp then
			txp_path = packages:get_list()[tab_selected_pkg].path
		end

		core.settings:set("texture_path", txp_path)
		packages = nil

		mm_game_theme.init()
		mm_game_theme.reset()
		return true
	end

	return false
end

function create_csm_dlg()
	mm_game_theme.set_engine()
	return dialog_create(
						"csm",
						get_formspec,
						handle_buttons,
						pkgmgr.update_gamelist
						)
end
