-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Modified based on dlg_reinstall_mtg.lua
-- Note that this is only needed for migrating from <5.11 to 5.12.

local doc_url = "https://docs.luanti.org/for-players/controls/"

local function get_formspec(dialogdata)
	local markup = table.concat({
		"<big>" .. hgettext("Keybindings changed") .. "</big>",
		hgettext("The internal input handling system was reworked in Luanti 5.12.0."),
		hgettext("As a result, your keybindings may have been changed."),
		hgettext("Refer to the following website for more information:"),
		("<action name='doc_url'><style color='cyan' hovercolor='orangered'>%s</style></action>"):format(doc_url),
	}, "\n")

	return table.concat({
		"formspec_version[6]",
		"size[12,7]",
		"hypertext[0.5,0.5;11,4.7;text;", core.formspec_escape(markup), "]",
		"container[0.5,5.7]",
		"style[dismiss;bgcolor=red]",
		"button[0,0;4,0.8;dismiss;", fgettext("Dismiss"), "]",
		"button[4.5,0;6.5,0.8;reconfigure;", fgettext("Configure keybindings"), "]",
		"container_end[]",
	})
end

local function buttonhandler(this, fields)
	if fields.reconfigure then
		this:delete()

		local maintab = ui.find_by_name("maintab")

		local dlg = create_settings_dlg("controls_keyboard_and_mouse")
		dlg:set_parent(maintab)
		maintab:hide()
		dlg:show()

		return true
	end

	if fields.dismiss then
		this:delete()
		return true
	end

	if fields.text == "action:doc_url" then
		core.open_url(doc_url)
	end
end

local function eventhandler(event)
	if event == "DialogShow" then
		mm_game_theme.set_engine()
		return true
	elseif event == "MenuQuit" then
		-- Don't allow closing the dialog with ESC, but still allow exiting
		-- Luanti
		core.close()
		return true
	end
	return false
end

local function create_rebind_keys_dlg()
	local dlg = dialog_create("dlg_rebind_keys", get_formspec,
			buttonhandler, eventhandler)
	return dlg
end

function migrate_keybindings()
	local has_migration = false
	local settings = core.settings:to_table()
	for name, value in pairs(settings) do
		if name:match("^keymap_") then
			local normalized = core.normalize_keycode(value)
			if value ~= normalized then
				has_migration = true
				core.settings:set(name, normalized)
			end
		end
	end

	if not has_migration then
		return
	end

	local maintab = ui.find_by_name("maintab")

	local dlg = create_rebind_keys_dlg()
	dlg:set_parent(maintab)
	maintab:hide()
	dlg:show()
	ui.update()
end
