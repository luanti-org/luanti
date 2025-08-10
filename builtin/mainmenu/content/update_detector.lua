-- Luanti
-- Copyright (C) 2023 rubenwardy
-- SPDX-License-Identifier: LGPL-2.1-or-later


update_detector = {}


if not core.get_http_api then
	update_detector.get_all = function() return {} end
	update_detector.get_count = function() return 0 end
	return
end


assert(core.create_dir(core.get_cache_path() .. DIR_DELIM .. "cdb"))
local cache_file_path = core.get_cache_path() .. DIR_DELIM .. "cdb" .. DIR_DELIM .. "updates.json"
local has_fetched = false
local latest_releases
do
	if check_cache_age("cdb_updates_last_checked", 24 * 3600) then
		local f = io.open(cache_file_path, "r")
		local data = ""
		if f then
			data = f:read("*a")
			f:close()
		end
		data = data ~= "" and core.parse_json(data) or nil
		if type(data) == "table" then
			latest_releases = data
			has_fetched = true
		end
	end
end


local function fetch_latest_releases()
	local version = core.get_version()
	local base_url = core.settings:get("contentdb_url")
	local url = base_url ..
			"/api/updates/?type=mod&type=game&type=txp&protocol_version=" ..
			core.get_max_supp_proto() .. "&engine_version=" .. core.urlencode(version.string)
	local http = core.get_http_api()
	local response = http.fetch_sync({ url = url })
	if not response.succeeded then
		return
	end

	return core.parse_json(response.data)
end


--- Get a table from package key (author/name) to latest release id
---
--- @param callback function that takes a single argument, table or nil
local function get_latest_releases(callback)
	core.handle_async(fetch_latest_releases, nil, callback)
end


local function has_packages_from_cdb()
	for _, content in pairs(pkgmgr.get_all()) do
		if pkgmgr.get_contentdb_id(content) then
			return true
		end
	end
	return false
end


--- @returns a new table with all keys lowercase
local function lowercase_keys(tab)
	local ret = {}
	for key, value in pairs(tab) do
		ret[key:lower()] = value
	end
	return ret
end


local function fetch()
	if has_fetched or not has_packages_from_cdb() then
		return
	end

	has_fetched = true

	get_latest_releases(function(releases)
		if not releases then
			has_fetched = false
			return
		end
		latest_releases = lowercase_keys(releases)
		core.safe_file_write(cache_file_path, core.write_json(latest_releases))
		cache_settings:set("cdb_updates_last_checked", tostring(os.time()))

		if update_detector.get_count() > 0 then
			local maintab = ui.find_by_name("maintab")
			if not maintab.hidden then
				ui.update()
			end
		end
	end)
end


--- @returns a list of content with an update available
function update_detector.get_all()
	if latest_releases == nil then
		fetch()
		return {}
	end

	local ret = {}
	local all_content = pkgmgr.get_all()
	for _, content in ipairs(all_content) do
		assert(content.path and content.path ~= "")
		local cdb_id = pkgmgr.get_contentdb_id(content)

		-- Do not consider content that we cannot modify to be out-of-date.
		-- This would be technically correct but confusing for the user.
		if cdb_id and core.may_modify_path(content.path) then
			-- The backend will account for aliases in `latest_releases`
			local latest_release = latest_releases[cdb_id]
			if not latest_release and content.type == "game" then
				latest_release = latest_releases[cdb_id .. "_game"]
			end

			if latest_release and latest_release > content.release then
				ret[#ret + 1] = content
			end
		end
	end

	return ret
end


--- @return number of packages with updates available
function update_detector.get_count()
	return #update_detector.get_all()
end
