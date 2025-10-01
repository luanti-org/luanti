-- Luanti
-- Copyright (C) 2025 siliconsniffer
-- SPDX-License-Identifier: LGPL-2.1-or-later


local function exit_dialog_formspec()
	local show_dialog = core.settings:get_bool("enable_esc_dialog", true)
	local formspec = {
		"size[10,3,true]" ..
		"label[0.5,0.5;" .. fgettext("Are you sure you want to quit?") .. "]" ..
		"checkbox[0.5,1;cb_show_dialog;" .. fgettext("Always show this dialog.") .. ";" .. tostring(show_dialog) .. "]" ..
		"style[btn_quit_confirm_yes;bgcolor=red]" ..
		"button[0.5,2.0;2.5,0.5;btn_quit_confirm_yes;" .. fgettext("Quit") .. "]" ..
		"button[7.0,2.0;2.5,0.5;btn_quit_confirm_cancel;" .. fgettext("Cancel") .. "]"
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
	elseif fields.btn_quit_confirm_cancel or fields.key_escape or fields.quit then
		this:delete()
		if tabview and tabview.show then
			tabview:show()
		end
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
