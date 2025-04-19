-- Luanti
-- Copyright (C) 2018-24 rubenwardy
-- SPDX-License-Identifier: LGPL-2.1-or-later

function get_formspec(data)
	local package = data.package

	return confirmation_formspec(
			fgettext("\"$1\" already exists. Would you like to overwrite it?", package.name),
			'install', fgettext("Overwrite"),
			'cancel', fgettext("Cancel"))
end


local function handle_submit(this, fields)
	local data = this.data
	if fields.cancel then
		this:delete()
		return true
	end

	if fields.install then
		this:delete()
		data.callback()
		return true
	end

	return false
end


function create_confirm_overwrite(package, callback)
	assert(type(package) == "table")
	assert(type(callback) == "function")

	local dlg = dialog_create("data", get_formspec, handle_submit, nil)
	dlg.data.package = package
	dlg.data.callback = callback
	return dlg
end
