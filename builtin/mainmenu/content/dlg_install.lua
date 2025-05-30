-- Luanti
-- Copyright (C) 2018-24 rubenwardy
-- SPDX-License-Identifier: LGPL-2.1-or-later

local function is_still_visible(dlg)
	local this = ui.find_by_name("install_dialog")
	return this == dlg and not dlg.hidden
end


local function get_loading_formspec()
	local TOUCH_GUI = core.settings:get_bool("touch_gui")
	local w = TOUCH_GUI and 14 or 7

	local formspec = {
		"formspec_version[3]",
		"size[", w, ",9.05]",
		TOUCH_GUI and "padding[0.01,0.01]" or "position[0.5,0.55]",
		"label[3,4.525;", fgettext("Loading..."), "]",
	}
	return table.concat(formspec)
end


local function get_formspec(data)
	if not data.has_hard_deps_ready then
		return get_loading_formspec()
	end

	local selected_game, selected_game_idx = pkgmgr.find_by_gameid(core.settings:get("menu_last_game"))
	if not selected_game_idx then
		selected_game_idx = 1
		selected_game = pkgmgr.games[1]
	end

	local game_list = {}
	for i, game in ipairs(pkgmgr.games) do
		game_list[i] = core.formspec_escape(game.title)
	end

	if not data.deps_ready[selected_game_idx] and
			not data.deps_loading[selected_game_idx] then
		data.deps_loading[selected_game_idx] = true

		contentdb.resolve_dependencies(data.package, selected_game, function(deps)
			if not is_still_visible(data.dlg) then
				return
			end
			data.deps_ready[selected_game_idx] = deps
			ui.update()
		end)
	end

	-- The value of `data.deps_ready[selected_game_idx]` may have changed
	-- since the last if statement since `contentdb.resolve_dependencies`
	-- calls the callback immediately if the dependencies are already cached.
	if not data.deps_ready[selected_game_idx] then
		return get_loading_formspec()
	end

	local package = data.package
	local will_install_deps = data.will_install_deps

	local deps_to_install = 0
	local deps_not_found = 0

	data.deps_chosen = data.deps_ready[selected_game_idx]
	local formatted_deps = {}
	for _, dep in pairs(data.deps_chosen) do
		formatted_deps[#formatted_deps + 1] = "#fff"
		formatted_deps[#formatted_deps + 1] = core.formspec_escape(dep.name)
		if dep.installed then
			formatted_deps[#formatted_deps + 1] = "#ccf"
			formatted_deps[#formatted_deps + 1] = fgettext("Already installed")
		elseif dep.package then
			formatted_deps[#formatted_deps + 1] = "#cfc"
			formatted_deps[#formatted_deps + 1] = fgettext("$1 by $2", dep.package.title, dep.package.author)
			deps_to_install = deps_to_install + 1
		else
			formatted_deps[#formatted_deps + 1] = "#f00"
			formatted_deps[#formatted_deps + 1] = fgettext("Not found")
			deps_not_found = deps_not_found + 1
		end
	end

	local message_bg = "#3333"
	local message
	if will_install_deps then
		message = fgettext("$1 and $2 dependencies will be installed.", package.title, deps_to_install)
	else
		message = fgettext("$1 will be installed, and $2 dependencies will be skipped.", package.title, deps_to_install)
	end
	if deps_not_found > 0 then
		message = fgettext("$1 required dependencies could not be found.", deps_not_found) ..
				" " .. fgettext("Please check that the base game is correct.", deps_not_found) ..
				"\n" .. message
		message_bg = mt_color_orange
	end

	local TOUCH_GUI = core.settings:get_bool("touch_gui")

	local w = TOUCH_GUI and 14 or 7
	local padded_w = w - 2*0.375
	local dropdown_w = TOUCH_GUI and 10.2 or 4.25
	local button_w = (padded_w - 0.25) / 3
	local button_pad = button_w / 2

	local formspec = {
		"formspec_version[3]",
		"size[", w, ",9.05]",
		TOUCH_GUI and "padding[0.01,0.01]" or "position[0.5,0.55]",
		"style[title;border=false]",
		"box[0,0;", w, ",0.8;#3333]",
		"button[0,0;", w, ",0.8;title;", fgettext("Install $1", package.title) , "]",

		"container[0.375,1]",

		"label[0,0.4;", fgettext("Base Game:"), "]",
		"dropdown[", padded_w - dropdown_w, ",0;", dropdown_w, ",0.8;selected_game;",
				table.concat(game_list, ","), ";", selected_game_idx, "]",

		"label[0,1.1;", fgettext("Dependencies:"), "]",

		"tablecolumns[color;text;color;text]",
		"table[0,1.4;", padded_w, ",3;packages;", table.concat(formatted_deps, ","), "]",

		"container_end[]",

		"checkbox[0.375,5.7;will_install_deps;",
		fgettext("Install missing dependencies"), ";",
		will_install_deps and "true" or "false", "]",

		"box[0,6;", w, ",1.8;", message_bg, "]",
		"textarea[0.375,6.1;", padded_w, ",1.6;;;", message, "]",

		"container[", 0.375 + button_pad, ",8.05]",
		"button[0,0;", button_w, ",0.8;install_all;", fgettext("Install"), "]",
		"button[", 0.25 + button_w, ",0;", button_w, ",0.8;cancel;", fgettext("Cancel"), "]",
		"container_end[]",
	}

	return table.concat(formspec)
end


local function handle_submit(this, fields)
	local data = this.data
	if fields.cancel then
		this:delete()
		return true
	end

	if fields.will_install_deps ~= nil then
		data.will_install_deps = core.is_yes(fields.will_install_deps)
		return true
	end

	if fields.install_all then
		contentdb.queue_download(data.package, contentdb.REASON_NEW)

		if data.will_install_deps then
			for _, dep in pairs(data.deps_chosen) do
				if not dep.is_optional and not dep.installed and dep.package then
					contentdb.queue_download(dep.package, contentdb.REASON_DEPENDENCY)
				end
			end
		end

		this:delete()
		return true
	end

	if fields.selected_game then
		for _, game in pairs(pkgmgr.games) do
			if game.title == fields.selected_game then
				core.settings:set("menu_last_game", game.id)
				break
			end
		end
		return true
	end

	return false
end


local function load_deps(dlg)
	local package = dlg.data.package

	contentdb.has_hard_deps(package, function(result)
		if not is_still_visible(dlg) then
			return
		end

		if result == nil then
			local parent = dlg.parent
			dlg:delete()
			local dlg2 = messagebox("error_checking_deps",
					fgettext("Error getting dependencies for package $1", package.url_part))
			dlg2:set_parent(parent)
			parent:hide()
			dlg2:show()
		elseif result == false then
			contentdb.queue_download(package, package.path and contentdb.REASON_UPDATE or contentdb.REASON_NEW)
			dlg:delete()
		else
			assert(result == true)
			dlg.data.has_hard_deps_ready = true
		end
		ui.update()
	end)
end


function create_install_dialog(package)
	local dlg = dialog_create("install_dialog", get_formspec, handle_submit, nil)
	dlg.data.deps_chosen = nil
	dlg.data.package = package
	dlg.data.will_install_deps = true

	dlg.data.has_hard_deps_ready = false
	dlg.data.deps_ready = {}
	dlg.data.deps_loading = {}

	dlg.load_deps = load_deps

	-- `get_formspec` needs to access `dlg` to check whether it's still open.
	-- It doesn't suffice to check that any "install_dialog" instance is open
	-- via `ui.find_by_name`, it's necessary to check for this exact instance.
	dlg.data.dlg = dlg

	return dlg
end


function install_or_update_package(parent, package)
	local install_parent
	if package.type == "mod" then
		install_parent = core.get_modpath()
	elseif package.type == "game" then
		install_parent = core.get_gamepath()
	elseif package.type == "txp" then
		install_parent = core.get_texturepath()
	else
		error("Unknown package type: " .. package.type)
	end

	if package.queued or package.downloading then
		return
	end

	local function on_confirm()
		local dlg = create_install_dialog(package)
		dlg:set_parent(parent)
		parent:hide()
		dlg:show()

		dlg:load_deps()
	end

	if package.type == "mod" and #pkgmgr.games == 0 then
		local dlg = messagebox("install_game",
				fgettext("You need to install a game before you can install a mod"))
		dlg:set_parent(parent)
		parent:hide()
		dlg:show()
	elseif not package.path and core.is_dir(install_parent .. DIR_DELIM .. package.name) then
		local dlg = create_confirm_overwrite(package, on_confirm)
		dlg:set_parent(parent)
		parent:hide()
		dlg:show()
	else
		on_confirm()
	end
end
