-- Luanti
-- Copyright (C) 2025 Bedwizen
-- SPDX-License-Identifier: LGPL-2.1-or-later

--------------------------------------------------------------------------------
--Ui setiings
local function rename_world_formspec(dialogdata)
	local retval =
		"size[11.5,4.5,true]" ..
		"button[2.2,3;2.5,0.5;dlg_rename_world_confirm;" ..
				fgettext("Confirm") .. "]" ..
		"button[6.7,3;2.5,0.5;dlg_rename_world_cancel;" ..
				fgettext("Cancel") .. "]"


	retval = retval ..
		"field[2.5,2;7,0.5;te_world_name;" ..
		fgettext("Rename World:") .. ";" .. core.formspec_escape(dialogdata.current_name) .. "]"

	if dialogdata.error_msg then
		retval = retval .. "label[2.5,3.8;" .. core.formspec_escape(dialogdata.error_msg) .. "]"
	end

	return retval
end

--------------------------------------------------------------------------------
-- Button handler
local function rename_world_buttonhandler(this, fields)

	-- press confirm button or Enter
	if fields["dlg_rename_world_confirm"]
		or fields.key_enter then
		local new_name = fields["te_world_name"]

		if not new_name
		or new_name == "" then
			this.data.error_msg = fgettext("World name can not be empty!")
			this.data.current_name = new_name
			core.update_formspec(this:get_formspec())
			return true
		end

		if new_name == this.data.old_name then
			this:delete()
			return true
		end

		if menudata.worldlist:uid_exists_raw(new_name) then
			this.data.error_msg = fgettext_ne("A world named \"$1\" already exists", new_name)
			this.data.current_name = new_name
			core.update_formspec(this:get_formspec())
			return true
		end


		local world = menudata.worldlist:get_raw_element(this.data.world_index)
		local err = core.rename_world(world.path, new_name)
		if err then
			this.data.error_msg = fgettext_ne(err)
			this.data.current_name = new_name
			core.update_formspec(this:get_formspec())
		else
			menudata.worldlist:refresh()
			this:delete()
		end

		return true
	end

	-- press cancel button or Esc
	if fields["dlg_rename_world_cancel"]
		or fields.key_escape then
		this:delete()
		return true
	end

	return false
end

--------------------------------------------------------------------------------
function create_rename_world_dlg(old_name, world_index)

	local retval = dialog_create("dlg_rename_world",
					rename_world_formspec,
					rename_world_buttonhandler,
					nil)
	retval.data = {
		old_name = old_name,
		current_name = old_name,
		world_index = world_index
	}
	return retval
end