-- Luanti
-- Copyright (C) 2026 rubenwardy
-- SPDX-License-Identifier: LGPL-2.1-or-later

function get_formspec(data)
	local package = data.package

	local title = fgettext("Share your installed packages list")
	local message =
		fgettext_ne("Help other users find content by sharing which packages you have installed and which games you use them with.") .. "\n\n" ..
		fgettext_ne("The report will not include any personally identifiable information, but a random user id will be generated to avoid duplicate reports.") .. "\n\n" ..
		fgettext_ne("You can change this setting later in the settings dialog.")
	local privacy_policy_url = "https://www.luanti.org/app-privacy-policy/#contentdb"

	local fs = {
		"size[10,7.5]",
		"style_type[label;textcolor=", core.formspec_escape(mt_color_green), "]",
		"style[privacy_policy;border=false;textcolor=", core.formspec_escape(mt_color_lightblue), "]",
		"label[0.375,0.5;", title, "]",
		"textarea[0.375,1.5;9.25,4.5;;;", core.formspec_escape(message), "]",
		"button[0.5,6;2.5,1;accept;", fgettext("Accept and share"), "]",
		"button_url[3.75,6;2.5,1;privacy_policy;", fgettext("Privacy Policy"), ";", core.formspec_escape(privacy_policy_url), "]",
		"button[7.0,6;2.5,1;deny;", fgettext("Deny"), "]"
	}

	return table.concat(fs, "")
end


local function handle_submit(this, fields)
	local data = this.data
	if fields.deny then
		contentdb.disable_package_reporting()
		this:delete()
		return true
	end

	if fields.accept then
		contentdb.enable_package_reporting()
		this:delete()
		return true
	end

	return false
end


function create_package_reporting_consent()
	local dlg = dialog_create("dlg_package_reporting_consent", get_formspec, handle_submit, nil)
	return dlg
end
