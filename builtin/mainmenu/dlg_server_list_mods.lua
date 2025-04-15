-- Luanti
-- Copyright (C) 2024 cx384
-- SPDX-License-Identifier: LGPL-2.1-or-later

local function get_formspec(dialogdata)
	local TOUCH_GUI = core.settings:get_bool("touch_gui")
	local server = dialogdata.server
	local group_by_prefix = dialogdata.group_by_prefix
	local expand_all = dialogdata.expand_all

	-- A wrongly behaving server may send ill formed mod names
	table.sort(server.mods)

	local cells = {}
	if group_by_prefix then
		local function get_prefix(mod)
			return mod:match("[^_]*")
		end
		local count = {}
		for _, mod in ipairs(server.mods) do
			local prefix = get_prefix(mod)
			count[prefix] = (count[prefix] or 0) + 1
		end
		local last_prefix
		local function add_row(depth, mod)
			table.insert(cells, ("%d"):format(depth))
			table.insert(cells, mod)
		end
		for i, mod in ipairs(server.mods) do
			local prefix = get_prefix(mod)
			if last_prefix == prefix then
				add_row(1, mod)
			elseif count[prefix] > 1 then
				add_row(0, prefix)
				add_row(1, mod)
			else
				add_row(0, mod)
			end
			last_prefix = prefix
		end
	else
		cells = table.copy(server.mods)
	end

	for i, cell in ipairs(cells) do
		cells[i] = core.formspec_escape(cell)
	end
	cells = table.concat(cells, ",")

	local heading
	if server.gameid then
		heading = fgettext("The $1 server uses a game called $2 and the following mods:",
			"<b>" .. core.hypertext_escape(server.name) .. "</b>",
			"<style font=mono>" .. core.hypertext_escape(server.gameid) .. "</style>")
	else
		heading = fgettext("The $1 server uses the following mods:",
				"<b>" .. core.hypertext_escape(server.name) .. "</b>")
	end

	local formspec = {
		"formspec_version[8]",
		"size[8,9.5]",
		TOUCH_GUI and "padding[0.01,0.01]" or "",
		"hypertext[0,0;8,1.5;;<global margin=5 halign=center valign=middle>", heading, "]",
		"tablecolumns[", group_by_prefix and
			(expand_all and "indent;text" or "tree;text") or "text", "]",
		"table[0.5,1.5;7,6.8;mods;", cells, "]",
		"checkbox[0.5,8.7;group_by_prefix;", fgettext("Group by prefix"), ";",
			group_by_prefix and "true" or "false", "]",
		group_by_prefix and ("checkbox[0.5,9.15;expand_all;" .. fgettext("Expand all") .. ";" ..
			(expand_all and "true" or "false") .. "]") or "",
		"button[5.5,8.5;2,0.8;quit;OK]"
	}
	return table.concat(formspec, "")
end

local function buttonhandler(this, fields)
	if fields.quit then
		this:delete()
		return true
	end

	if fields.group_by_prefix then
		this.data.group_by_prefix = core.is_yes(fields.group_by_prefix)
		return true
	end

	if fields.expand_all then
		this.data.expand_all = core.is_yes(fields.expand_all)
		return true
	end

	return false
end

function create_server_list_mods_dialog(server)
	local retval = dialog_create("dlg_server_list_mods",
		get_formspec,
		buttonhandler,
		nil)
	retval.data.group_by_prefix = false
	retval.data.expand_all = false
	retval.data.server = server
	return retval
end
