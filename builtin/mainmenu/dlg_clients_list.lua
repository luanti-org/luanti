-- Luanti
-- Copyright (C) 2024 siliconsniffer
-- SPDX-License-Identifier: LGPL-2.1-or-later


local function clients_list_formspec(dialogdata)
	local TOUCH_GUI = core.settings:get_bool("touch_gui")
	local clients_list = dialogdata.server.clients_list
	local servername   = dialogdata.server.name

	local function fmt_formspec_list(clients_list)
		local escaped = {}
		for i, str in ipairs(clients_list) do
			escaped[i] = core.formspec_escape(str)
		end
		return table.concat(escaped, ",")
	end

	local formspec = {
		"formspec_version[8]",
		"size[6,9.5]",
		TOUCH_GUI and "padding[0.01,0.01]" or "",
		"hypertext[0,0;6,1.5;;<global margin=5 halign=center valign=middle>",
			fgettext("Players connected to\n$1",
				"<b>" .. core.hypertext_escape(servername) .. "</b>") .. "]",
		"textlist[0.5,1.5;5,6.8;;" .. fmt_formspec_list(clients_list) .. "]",
		"button[1.5,8.5;3,0.8;quit;OK]"
	}
	return table.concat(formspec, "")
end


local function clients_list_buttonhandler(this, fields)
	if fields.quit then
		this:delete()
		return true
	end
	return false
end


function create_clientslist_dialog(server)
	local retval = dialog_create("dlg_clients_list",
		clients_list_formspec,
		clients_list_buttonhandler,
		nil)
	retval.data.server = server
	return retval
end
