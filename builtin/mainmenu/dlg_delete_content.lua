-- Luanti
-- Copyright (C) 2014 sapier
-- SPDX-License-Identifier: LGPL-2.1-or-later

--------------------------------------------------------------------------------

local function delete_content_formspec(dialogdata)
	return confirmation_formspec(
		fgettext("Are you sure you want to delete \"$1\"?", dialogdata.content.name),
		'dlg_delete_content_confirm', fgettext("Delete"),
		'dlg_delete_content_cancel', fgettext("Cancel"))
end

--------------------------------------------------------------------------------
local function delete_content_buttonhandler(this, fields)
	if fields["dlg_delete_content_confirm"] ~= nil then

		if this.data.content.path ~= nil and
				this.data.content.path ~= "" and
				this.data.content.path ~= core.get_modpath() and
				this.data.content.path ~= core.get_gamepath() and
				this.data.content.path ~= core.get_texturepath() then
			if not core.delete_dir(this.data.content.path) then
				gamedata.errormessage = fgettext_ne("pkgmgr: failed to delete \"$1\"", this.data.content.path)
			end

			pkgmgr.reload_by_type(this.data.content.type)
		else
			gamedata.errormessage = fgettext_ne("pkgmgr: invalid path \"$1\"", this.data.content.path)
		end
		this:delete()
		return true
	end

	if fields["dlg_delete_content_cancel"] then
		this:delete()
		return true
	end

	return false
end

--------------------------------------------------------------------------------
function create_delete_content_dlg(content)
	assert(content.name)

	local retval = dialog_create("dlg_delete_content",
					delete_content_formspec,
					delete_content_buttonhandler,
					nil)
	retval.data.content = content
	return retval
end
