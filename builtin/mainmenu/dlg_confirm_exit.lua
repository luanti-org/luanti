-- Luanti
-- Copyright (C) 2025 siliconsniffer
-- SPDX-License-Identifier: LGPL-2.1-or-later


local function exit_dialog_formspec()
	local show_dialog = core.settings:get_bool("enable_esc_dialog", true)
	local formspec = {
		"formspec_version[10]" ..
		"size[10,3.6]" ..
		"style_type[label;font=bold]" ..
		"style[btn_quit_confirm_yes;bgcolor=red]" ..
		"label[0.5,0.5;" .. fgettext("Are you sure you want to quit?") .. "]" ..
		"checkbox[0.5,1.4;cb_show_dialog;" .. fgettext("Always show this dialog.") .. ";" .. tostring(show_dialog) .. "]" ..
		"button[0.5,2.3;3,0.8;btn_quit_confirm_cancel;" .. fgettext("Cancel") .. "]" ..
		"button[6.5,2.3;3,0.8;btn_quit_confirm_yes;" .. fgettext("Quit") .. "]" ..
		"set_focus[btn_quit_confirm_yes]"
	}
	return table.concat(formspec, "")
end


local function exit_dialog_buttonhandler(this, fields)
	if fields.cb_show_dialog ~= nil then
		core.settings:set_bool("enable_esc_dialog", core.is_yes(fields.cb_show_dialog))
		return false
	elseif fields.btn_quit_confirm_yes then
		this:delete()
		core.close()
		return true
	elseif fields.btn_quit_confirm_cancel then
		this:delete()
		this:show()
		return true
	end
end


local function event_handler(event)
	if event == "DialogShow" then
		mm_game_theme.set_engine(true) -- hide the menu header
		return true
	end
	return false
end


function create_exit_dialog()
	local retval = dialog_create("dlg_exit",
		exit_dialog_formspec,
		exit_dialog_buttonhandler,
		event_handler)
	return retval
end
