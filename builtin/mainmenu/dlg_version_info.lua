--[[
Luanti
Copyright (C) 2018-2020 SmallJoker, 2022 rubenwardy

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
]]

if not core.get_http_api then
	function check_new_version()
	end
	return
end

local function version_info_formspec(data)
	local cur_ver = core.get_version()
	local title = fgettext("A new $1 version is available", cur_ver.project)
	local message =
		fgettext("Installed version: $1\nNew version: $2\n" ..
				"Visit $3 to find out how to get the newest version and stay up to date" ..
				" with features and bugfixes.",
			cur_ver.string, data.new_version or "", data.url or "")

	local fs = {
		"formspec_version[3]",
		"size[12.8,7]",
		"style_type[label;textcolor=#0E0]",
		"label[0.5,0.8;", title, "]",
		"textarea[0.4,1.6;12,3.4;;;", message, "]",
		"container[0.4,5.8]",
		"button[0.0,0;4.0,0.8;version_check_visit;", fgettext("Visit website"), "]",
		"button[4.5,0;3.5,0.8;version_check_remind;", fgettext("Later"), "]",
		"button[8.5.5,0;3.5,0.8;version_check_never;", fgettext("Never"), "]",
		"container_end[]",
	}

	return table.concat(fs, "")
end

local function version_info_buttonhandler(this, fields)
	if fields.version_check_remind then
		-- Erase last known, user will be reminded again at next check
		cache_settings:set("update_last_known", "")
		this:delete()
		return true
	end
	if fields.version_check_never then
		-- clear checked URL
		core.settings:set("update_information_url", "")
		this:delete()
		return true
	end
	if fields.version_check_visit then
		if type(this.data.url) == "string" then
			core.open_url(this.data.url)
		end
		this:delete()
		return true
	end

	return false
end

local function version_info_eventhandler(event)
	if event == "DialogShow" then
		mm_game_theme.set_engine()
		return true
	end

	return false
end

local function create_version_info_dlg(new_version, url)
	assert(type(new_version) == "string")
	assert(type(url) == "string")

	local retval = dialog_create("version_info",
		version_info_formspec,
		version_info_buttonhandler,
		version_info_eventhandler)

	retval.data.new_version = new_version
	retval.data.url = url

	return retval
end

local function get_current_version_code()
	-- Format: Major.Minor.Patch
	-- Convert to MMMNNNPPP
	local cur_string = core.get_version().string
	local cur_major, cur_minor, cur_patch = cur_string:match("^(%d+).(%d+).(%d+)")

	if not cur_patch then
		core.log("error", "Failed to parse version numbers (invalid tag format?)")
		return
	end

	return (cur_major * 1000 + cur_minor) * 1000 + cur_patch
end

local function on_version_info_received(json)
	local maintab = ui.find_by_name("maintab")
	if maintab.hidden then
		-- Another dialog is open, abort.
		return
	end

	local known_update = tonumber(cache_settings:get("update_last_known")) or 0

	-- Format: MMNNPPP (Major, Minor, Patch)
	local new_number = type(json.latest) == "table" and json.latest.version_code
	if type(new_number) ~= "number" then
		core.log("error", "Failed to read version number (invalid response?)")
		return
	end

	local cur_number = get_current_version_code()
	if new_number <= known_update or new_number < cur_number then
		return
	end

	-- Also consider updating from 1.2.3-dev to 1.2.3
	if new_number == cur_number and not core.get_version().is_dev then
		return
	end

	cache_settings:set("update_last_known", tostring(new_number))

	-- Show version info dialog (once)
	maintab:hide()

	local version_info_dlg = create_version_info_dlg(json.latest.version, json.latest.url)
	version_info_dlg:set_parent(maintab)
	version_info_dlg:show()

	ui.update()
end

function check_new_version()
	local url = core.settings:get("update_information_url")
	if url == "" then
		-- Never show any updates
		return
	end

	-- every 2 days
	if check_cache_age("update_last_checked", 2 * 24 * 3600) then
		return
	end
	cache_settings:set("update_last_checked", tostring(os.time()))

	-- Clean old leftovers (this can be removed after 5.9.0 or so)
	core.settings:remove("update_last_checked")
	core.settings:remove("update_last_known")

	core.handle_async(function(params)
		local http = core.get_http_api()
		return http.fetch_sync(params)
	end, { url = url }, function(result)
		local json = result.succeeded and core.parse_json(result.data)
		if type(json) ~= "table" or not json.latest then
			core.log("error", "Failed to read JSON output from " .. url ..
					", status code = " .. result.code)
			return
		end

		on_version_info_received(json)
	end)
end
