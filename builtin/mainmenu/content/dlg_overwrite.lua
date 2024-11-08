--Luanti
--Copyright (C) 2018-24 rubenwardy
--
--This program is free software; you can redistribute it and/or modify
--it under the terms of the GNU Lesser General Public License as published by
--the Free Software Foundation; either version 2.1 of the License, or
--(at your option) any later version.
--
--This program is distributed in the hope that it will be useful,
--but WITHOUT ANY WARRANTY; without even the implied warranty of
--MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--GNU Lesser General Public License for more details.
--
--You should have received a copy of the GNU Lesser General Public License along
--with this program; if not, write to the Free Software Foundation, Inc.,
--51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

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
