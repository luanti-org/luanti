-- Luanti
-- Copyright (C) 2014 sapier
-- SPDX-License-Identifier: LGPL-2.1-or-later

--------------------------------------------------------------------------------

local function rename_modpack_formspec(dialogdata)
	local retval =
		"size[11.5,4.5,true]" ..
		"button[3.25,3.5;2.5,0.5;dlg_rename_modpack_confirm;"..
				fgettext("Accept") .. "]" ..
		"button[5.75,3.5;2.5,0.5;dlg_rename_modpack_cancel;"..
				fgettext("Cancel") .. "]"

	local input_y = 2
	if dialogdata.mod.is_name_explicit then
		retval = retval .. "textarea[1,0.2;10,2;;;" ..
				fgettext("This modpack has an explicit name given in its modpack.conf " ..
						"which will override any renaming here.") .. "]"
		input_y = 2.5
	end
	retval = retval ..
		"field[2.5," .. input_y .. ";7,0.5;te_modpack_name;" ..
		fgettext("Rename Modpack:") .. ";" .. dialogdata.mod.dir_name .. "]"

	return retval
end

--------------------------------------------------------------------------------
local function rename_modpack_buttonhandler(this, fields)
	if fields["dlg_rename_modpack_confirm"] ~= nil then
		local oldpath = this.data.mod.path
		local targetpath = this.data.mod.parent_dir .. DIR_DELIM .. fields["te_modpack_name"]
		os.rename(oldpath, targetpath)
		pkgmgr.reload_global_mods()
		pkgmgr.selected_mod = pkgmgr.global_mods:get_current_index(
			pkgmgr.global_mods:raw_index_by_uid(fields["te_modpack_name"]))

		this:delete()
		return true
	end

	if fields["dlg_rename_modpack_cancel"] then
		this:delete()
		return true
	end

	return false
end

--------------------------------------------------------------------------------
function create_rename_modpack_dlg(modpack)

	local retval = dialog_create("dlg_delete_mod",
					rename_modpack_formspec,
					rename_modpack_buttonhandler,
					nil)
	retval.data.mod = modpack
	return retval
end
