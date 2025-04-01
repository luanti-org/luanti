-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later

local keybinding_types = {
	"none", "key", "mouse",
	none = 1, key = 2, mouse = 3,
}

local mouse_buttons = {
	"KEY_LBUTTON", "KEY_MBUTTON", "KEY_RBUTTON", "KEY_XBUTTON1", "KEY_XBUTTON2",
}

local mouse_button_dropdown = {}
for k, v in pairs(mouse_buttons) do
	mouse_button_dropdown[k] = core.formspec_escape(core.get_keycode_name(mouse_buttons[k]))
end
mouse_button_dropdown = table.concat(mouse_button_dropdown, ",")

local keybinding_type_dropdown = table.concat({
	fgettext("No keybinding"),
	fgettext("Bind to key"),
	fgettext("Bind to mouse button"),
}, ",")

local function expand_keybinding_type_information(value, ignore_empty)
	if not ignore_empty and core.are_keycodes_equal(value, "") then
		return "none"
	end
	local index = table.indexof(mouse_buttons, value)
	if index ~= -1 then
		return "mouse", index
	end
	return "key", value
end

local function get_formspec(dialogdata)
	local name = dialogdata.setting.name
	local keytype, keyval = expand_keybinding_type_information(core.settings:get(name), dialogdata.ignore_empty)
	local readable_name = name
	if dialogdata.setting.readable_name then
		readable_name = ("%s (%s)"):format(fgettext(dialogdata.setting.readable_name), name)
	end

	local fs = {
		"formspec_version[4]",
		"", -- size element; height can only be determined later
		("label[0.5,0.8;%s]"):format(readable_name),
		("dropdown[0.5,1.1;5,0.8;typesel;%s;%d;true]"):format(
				keybinding_type_dropdown, keybinding_types[keytype]),
	}

	local height = 1.9
	if keytype == "key" then
		table.insert(fs, ("button_key[0.5,%f;5,0.8;btn_bind;%s]"):format(
				height, core.formspec_escape(keyval)))
		height = height + 0.8
	elseif keytype == "mouse" then
		table.insert(fs, ("dropdown[0.5,%f;5,0.8;mousesel;%s;%d;true]"):format(
				height, mouse_button_dropdown, keyval))
		height = height + 0.8
	end

	height = add_keybinding_conflict_warnings(fs, 0.5, height, name)
	table.insert(fs, ("button[2,%f;2,0.8;btn_close;%s]"):format(height+0.2, fgettext("Close")))
	fs[2] = ("size[6,%f]"):format(height+1.5)

	return table.concat(fs)
end

local function buttonhandler(self, fields)
	local name = self.data.setting.name
	local value = core.settings:get(name)
	if fields.quit or fields.btn_close then
		self:delete()
		return true
	elseif fields.btn_bind then
		core.settings:set(name, fields.btn_bind)
		return true
	elseif fields.mousesel and mouse_buttons[tonumber(fields.mousesel)] ~= value then
		core.settings:set(name, mouse_buttons[tonumber(fields.mousesel)])
		return true
	elseif fields.typesel then
		local index = keybinding_types[tonumber(fields.typesel)] or "none"
		if index == expand_keybinding_type_information(value, self.data.ignore_empty) then
			return false -- selected same keybinding type -> do not handle
		elseif index == "none" then
			core.settings:set(name, "")
			self.data.ignore_empty = nil
		elseif index == "key" then
			core.settings:set(name, "")
			self.data.ignore_empty = true
		elseif index == "mouse" then
			core.settings:set(name, mouse_buttons[1])
		end
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

-- This is also used by the setting component to display conflict warnings in the setting menu
function add_keybinding_conflict_warnings(fs, x, y, setting_name)
	local value = core.settings:get(setting_name)
	if value == "" then
		return y
	end
	for _, o in ipairs(core.full_settingtypes) do
		if o.type == "key" and o.name ~= setting_name
				and core.are_keycodes_equal(core.settings:get(o.name), value) then
			table.insert(fs, ("label[%f,%f;%s]"):format(x, y+0.3, core.colorize(mt_color_orange,
					fgettext([[Conflicts with "$1"]], fgettext(o.readable_name)))))
			y = y + 0.6
		end
	end
	return y
end
