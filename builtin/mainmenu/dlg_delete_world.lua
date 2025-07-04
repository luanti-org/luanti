-- Luanti
-- Copyright (C) 2014 sapier
-- SPDX-License-Identifier: LGPL-2.1-or-later


local function delete_world_formspec(dialogdata)
	return confirmation_formspec(
		fgettext("Delete World \"$1\"?", dialogdata.delete_name),
		'world_delete_confirm', fgettext("Delete"),
		'world_delete_cancel', fgettext("Cancel"))
end

local function delete_world_buttonhandler(this, fields)
	if fields["world_delete_confirm"] then
		if this.data.delete_index > 0 and
				this.data.delete_index <= #menudata.worldlist:get_raw_list() then
			core.delete_world(this.data.delete_index)
			menudata.worldlist:refresh()
		end
		this:delete()
		return true
	end

	if fields["world_delete_cancel"] then
		this:delete()
		return true
	end

	return false
end


function create_delete_world_dlg(name_to_del, index_to_del)
	assert(name_to_del ~= nil and type(name_to_del) == "string" and name_to_del ~= "")
	assert(index_to_del ~= nil and type(index_to_del) == "number")

	local retval = dialog_create("delete_world",
					delete_world_formspec,
					delete_world_buttonhandler,
					nil)
	retval.data.delete_name  = name_to_del
	retval.data.delete_index = index_to_del

	return retval
end
