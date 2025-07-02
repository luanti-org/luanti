-- Luanti
-- Copyright (C) 2014 sapier
-- SPDX-License-Identifier: LGPL-2.1-or-later

ui = {}
ui.childlist = {}
ui.default = nil
-- Whether fstk is currently showing its own formspec instead of active ui elements.
ui.overridden = false

--------------------------------------------------------------------------------
function ui.add(child)
	--TODO check child
	ui.childlist[child.name] = child

	return child.name
end

--------------------------------------------------------------------------------
function ui.delete(child)

	if ui.childlist[child.name] == nil then
		return false
	end

	ui.childlist[child.name] = nil
	return true
end

--------------------------------------------------------------------------------
function ui.set_default(name)
	ui.default = name
end

--------------------------------------------------------------------------------
function ui.find_by_name(name)
	return ui.childlist[name]
end

--------------------------------------------------------------------------------
-- "title" and "message" must already be formspec-escaped, e.g. via fgettext or
-- core.formspec_escape.
function ui.get_message_formspec(title, message, btn_id)
	return table.concat({
		"size[14,8]",
		"real_coordinates[true]",
		"set_focus[", btn_id, ";true]",
		"box[0.5,1.2;13,5;#000]",
		("textarea[0.5,1.2;13,5;;%s;%s]"):format(title, message),
		"button[5,6.6;4,1;", btn_id, ";" .. fgettext("OK") .. "]",
	})
end

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-- Internal functions not to be called from user
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

function ui.update()
	ui.overridden = false
	local formspec = {}

	-- handle errors
	if gamedata ~= nil and gamedata.reconnect_requested then
		local error_message = core.formspec_escape(gamedata.errormessage)
				or fgettext("<none available>")
		formspec = {
			"size[14,8]",
			"real_coordinates[true]",
			"set_focus[btn_reconnect_yes;true]",
			"box[0.5,1.2;13,5;#000]",
			("textarea[0.5,1.2;13,5;;%s;%s]"):format(
				fgettext("The server has requested a reconnect:"), error_message),
			"button[2,6.6;4,1;btn_reconnect_yes;" .. fgettext("Reconnect") .. "]",
			"button[8,6.6;4,1;btn_reconnect_no;" .. fgettext("Main menu") .. "]"
		}
		ui.overridden = true
	elseif gamedata ~= nil and gamedata.errormessage ~= nil then
		-- Note to API users:
		-- "gamedata.errormessage" must not be formspec-escaped yet.
		-- For translations, fgettext_ne should be used.
		local error_message = core.formspec_escape(gamedata.errormessage)

		local error_title
		if string.find(gamedata.errormessage, "ModError") then
			error_title = fgettext("An error occurred in a Lua script:")
		else
			error_title = fgettext("An error occurred:")
		end
		formspec = {ui.get_message_formspec(error_title, error_message, "btn_error_confirm")}
		ui.overridden = true
	else
		local active_toplevel_ui_elements = 0
		for key,value in pairs(ui.childlist) do
			if (value.type == "toplevel") then
				local retval = value:get_formspec()

				if retval ~= nil and retval ~= "" then
					active_toplevel_ui_elements = active_toplevel_ui_elements + 1
					table.insert(formspec, retval)
				end
			end
		end

		-- no need to show addons if there ain't a toplevel element
		if (active_toplevel_ui_elements > 0) then
			for key,value in pairs(ui.childlist) do
				if (value.type == "addon") then
					local retval = value:get_formspec()

					if retval ~= nil and retval ~= "" then
						table.insert(formspec, retval)
					end
				end
			end
		end

		if (active_toplevel_ui_elements > 1) then
			core.log("warning", "more than one active ui "..
				"element, this most likely isn't intended")
		end

		if (active_toplevel_ui_elements == 0) then
			core.log("warning", "no toplevel ui element "..
					"active; switching to default")
			ui.childlist[ui.default]:show()
			formspec = {ui.childlist[ui.default]:get_formspec()}
		end
	end
	core.update_formspec(table.concat(formspec))
end

--------------------------------------------------------------------------------
function ui.handle_buttons(fields)
	for key,value in pairs(ui.childlist) do

		local retval = value:handle_buttons(fields)

		if retval then
			ui.update()
			return
		end
	end
end


--------------------------------------------------------------------------------
function ui.handle_events(event)

	for key,value in pairs(ui.childlist) do

		if value.handle_events ~= nil then
			local retval = value:handle_events(event)

			if retval then
				return retval
			end
		end
	end
end

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-- initialize callbacks
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
core.button_handler = function(fields)
	if fields["try_quit"] and not fields["key_enter"] then
		core.event_handler("MenuQuit")
		return
	end
	if fields["btn_reconnect_yes"] then
		gamedata.reconnect_requested = false
		gamedata.errormessage = nil
		gamedata.do_reconnect = true
		core.start()
		return
	elseif fields["btn_reconnect_no"] or fields["btn_error_confirm"] then
		gamedata.errormessage = nil
		gamedata.reconnect_requested = false
		ui.update()
		return
	end

	if ui.handle_buttons(fields) then
		ui.update()
	end
end

--------------------------------------------------------------------------------
core.event_handler = function(event)
	-- Handle error messages
	if ui.overridden then
		if event == "MenuQuit" then
			gamedata.errormessage = nil
			gamedata.reconnect_requested = false
			ui.update()
		end
		return
	end

	if ui.handle_events(event) then
		ui.update()
		return
	end

	if event == "Refresh" then
		ui.update()
		return
	end
end
