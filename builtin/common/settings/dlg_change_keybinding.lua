-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
local function get_formspec(dialogdata)
	local name = dialogdata.setting.name
	local readable_name = name
	if dialogdata.setting.readable_name then
		readable_name = ("%s (%s)"):format(fgettext(dialogdata.setting.readable_name), name)
	end

	local fs = {
		"formspec_version[4]",
		"size[6,2]", -- size element; height can only be determined later
		("label[0.5,0.8;%s]"):format(readable_name),
	}

	return table.concat(fs)
end

local function buttonhandler(self, fields)
	local name = self.data.setting.name
	if fields.quit or fields.btn_close then
		self:delete()
		return true
	end
	return false
end

local formspec_handlers = {}
if INIT == "pause_menu" then
	core.register_on_formspec_input(function(formname, fields)
		if formspec_handlers[formname] then
			formspec_handlers[formname](fields)
			return true
		end
	end)
end

local is_mainmenu = INIT == "mainmenu"
function show_change_keybinding_dlg(setting, tabview)
	local dlg
	if is_mainmenu then
		dlg = dialog_create("dlg_change_keybinding",
				get_formspec, buttonhandler)
	else
		local name = "__builtin:rebind_" .. setting.name
		dlg = {
			show = function()
				if dlg.removed then
					--core.open_settings("controls_keyboard_and_mouse")
					tabview:show()
				else
					core.show_formspec(name, get_formspec(dlg.data))
				end
			end,
			delete = function()
				core.show_formspec(name, "")
				formspec_handlers[name] = nil
				dlg.removed = true
			end,
			data = {},
		}
		formspec_handlers[name] = function(fields)
			if buttonhandler(dlg, fields) then
				dlg:show()
			end
		end
	end

	dlg.data.setting = setting

	if is_mainmenu then
		dlg:set_parent(tabview)
		tabview:hide()
	end
	dlg:show()

	return is_mainmenu
end
