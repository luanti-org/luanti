-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
local function get_formspec(dialogdata)
	local name = dialogdata.setting.name
	local readable_name = name
	if dialogdata.setting.readable_name then
		readable_name = ("%s (%s)"):format(fgettext(dialogdata.setting.readable_name), name)
	end

	local value = dialogdata.value
	local selection = dialogdata.selection

	local fs = {
		"formspec_version[4]",
		"size[6,9]",
		("label[0.5,0.8;%s]"):format(readable_name),
		("button[0.5,5.7;5,0.8;btn_add;%s]"):format(fgettext("Add keybinding")),
		("button_key[0.5,6.7;4.2,0.8;btn_bind;%s]"):format(core.formspec_escape(value[selection] or "")),
		("image_button[4.6,6.7;0.8,0.8;%s;btn_clear;]"):format(
						core.formspec_escape(defaulttexturedir .. "clear.png")),
		("button[3.1,7.7;2.4,0.8;btn_close;%s]"):format(fgettext("Cancel")),
		("button[0.5,7.7;2.4,0.8;btn_save;%s]"):format(fgettext("Save")),
	}

	local cells = {}
	for _, key in ipairs(value) do
		table.insert(cells, core.formspec_escape(core.get_key_description(key)))
	end
	table.insert(fs, ("textlist[0.5,1.3;5,4;keylist;%s;%d;false]"):format(table.concat(cells, ","), selection))

	return table.concat(fs)
end

local function buttonhandler(self, fields)
	local name = self.data.setting.name
	if fields.quit or fields.btn_close then
		self:delete()
		return true
	elseif fields.btn_save then
		local value = {}
		for _, v in ipairs(self.data.value) do
			if v ~= "" then -- filter out "empty" keybindings
				table.insert(value, v)
			end
		end
		core.settings:set(name, table.concat(value, "|"))
		self:delete()
		return true
	elseif fields.btn_clear then
		local selection = self.data.selection
		table.remove(self.data.value, selection)
		self.data.selection = math.max(1, math.min(selection, #self.data.value))
		return true
	elseif fields.btn_add then
		table.insert(self.data.value, "")
		self.data.selection = #self.data.value
		return true
	elseif fields.btn_bind then
		self.data.value[self.data.selection] = fields.btn_bind
		return true
	elseif fields.keylist then
		local event = core.explode_textlist_event(fields.keylist)
		if event.type == "CHG" or event.type == "DCL" then
			self.data.selection = event.index
			return true
		end
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
	dlg.data.value = core.settings:get(setting.name):split("|")
	dlg.data.selection = 1

	if is_mainmenu then
		dlg:set_parent(tabview)
		tabview:hide()
	end
	dlg:show()

	return is_mainmenu
end
