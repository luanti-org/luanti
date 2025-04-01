-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later

local function get_formspec(dialogdata)
	local name = dialogdata.setting.name
	local value = core.settings:get(name)
	local readable_name = name
	if dialogdata.setting.readable_name then
		readable_name = ("%s (%s)"):format(fgettext(dialogdata.setting.readable_name), name)
	end

	local fs = {
		"formspec_version[4]",
		"", -- size element; height can only be determined later
		("label[0.5,0.8;%s]"):format(readable_name)
	}

	local height = 1.9
	table.insert(fs, ("button_key[0.5,1.1;5,0.8;btn_bind;%s]"):format(
			core.formspec_escape(value)))

	height = add_keybinding_conflict_warnings(fs, 0.5, height, name)
	table.insert(fs, ("button[2,%f;2,0.8;btn_close;%s]"):format(height+0.2, fgettext("Close")))
	fs[2] = ("size[6,%f]"):format(height+1.5)

	return table.concat(fs)
end

local function buttonhandler(self, fields)
	local name = self.data.setting.name
	if fields.quit or fields.btn_close then
		self:delete()
		return true
	elseif fields.btn_bind then
		core.settings:set(name, fields.btn_bind)
		return true
	end
	return false
end

function create_change_keybinding_dlg(setting)
	local retval = dialog_create("dlg_change_keybinding",
			get_formspec, buttonhandler)
	retval.data.setting = setting
	return retval
end

-- This is also used by the setting component to display conflict warnings in the setting menu
function add_keybinding_conflict_warnings(fs, x, y, setting_name)
	local value = core.settings:get(setting_name)
	if value == "" then
		return y
	end
	for _, o in pairs(core.full_settingtypes) do
		if o.type == "key" and o.name ~= setting_name
				and core.are_keycodes_equal(core.settings:get(o.name), value) then
			table.insert(fs, ("label[%f,%f;%s]"):format(x, y+0.3, core.colorize(mt_color_orange,
					fgettext([[Conflicts with "$1"]], fgettext(o.readable_name)))))
			y = y + 0.6
		end
	end
	return y
end
