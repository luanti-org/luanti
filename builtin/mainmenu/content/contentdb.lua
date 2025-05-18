-- Luanti
-- Copyright (C) 2018-24 rubenwardy
-- SPDX-License-Identifier: LGPL-2.1-or-later

if not core.get_http_api then
	return
end


contentdb = {
	loading = false,
	load_ok = false,
	load_error = false,

	-- Unordered preserves the original order of the ContentDB API,
	-- before the package list is ordered based on installed state.
	packages = {},
	packages_full = {},
	packages_full_unordered = {},
	package_by_id = {},
	aliases = {},

	number_downloading = 0,
	download_queue = {},

	REASON_NEW = "new",
	REASON_UPDATE = "update",
	REASON_DEPENDENCY = "dependency",
}

-- API documentation: https://content.luanti.org/help/api/

local function get_download_url(package, reason)
	local base_url = core.settings:get("contentdb_url")
	local ret = base_url .. ("/packages/%s/releases/%d/download/"):format(
			package.url_part, package.release)
	if reason then
		ret = ret .. "?reason=" .. reason
	end
	return ret
end


local function download_and_extract(param)
	local package = param.package

	local filename = core.get_temp_path(true)
	if filename == "" or not core.download_file(param.url, filename) then
		core.log("error", "Downloading " .. dump(param.url) .. " failed")
		return {
			msg = fgettext_ne("Failed to download \"$1\"", package.title)
		}
	end

	local tempfolder = core.get_temp_path()
	if tempfolder ~= "" and not core.extract_zip(filename, tempfolder) then
		tempfolder = ""
	end
	os.remove(filename)
	if tempfolder == "" then
		return {
			msg = fgettext_ne("Failed to extract \"$1\" " ..
					"(insufficient disk space, unsupported file type or broken archive)",
					package.title),
		}
	end

	return {
		path = tempfolder
	}
end


local function start_install(package, reason)
	local params = {
		package = package,
		url = get_download_url(package, reason),
	}

	contentdb.number_downloading = contentdb.number_downloading + 1

	local function callback(result)
		if result.msg then
			gamedata.errormessage = result.msg
		else
			local path, msg = pkgmgr.install_dir(package.type, result.path, package.name, package.path)
			core.delete_dir(result.path)
			if not path then
				gamedata.errormessage = fgettext_ne("Error installing \"$1\": $2", package.title, msg)
			else
				core.log("action", "Installed package to " .. path)

				local conf_path
				local name_is_title = false
				if package.type == "mod" then
					local actual_type = pkgmgr.get_folder_type(path)
					if actual_type.type == "modpack" then
						conf_path = path .. DIR_DELIM .. "modpack.conf"
					else
						conf_path = path .. DIR_DELIM .. "mod.conf"
					end
				elseif package.type == "game" then
					conf_path = path .. DIR_DELIM .. "game.conf"
					name_is_title = true
				elseif package.type == "txp" then
					conf_path = path .. DIR_DELIM .. "texture_pack.conf"
				end

				if conf_path then
					local conf = Settings(conf_path)
					if not conf:get("title") then
						conf:set("title", package.title)
					end
					if not name_is_title then
						conf:set("name", package.name)
					end
					if not conf:get("description") then
						conf:set("description", package.short_description)
					end
					conf:set("author",     package.author)
					conf:set("release",    package.release)
					conf:write()
				end

				pkgmgr.reload_by_type(package.type)
			end
		end

		package.downloading = false

		contentdb.number_downloading = contentdb.number_downloading - 1

		local next = contentdb.download_queue[1]
		if next then
			table.remove(contentdb.download_queue, 1)

			start_install(next.package, next.reason)
		end
		ui.update()
	end

	package.queued = false
	package.downloading = true

	if not core.handle_async(download_and_extract, params, callback) then
		core.log("error", "ERROR: async event failed")
		gamedata.errormessage = fgettext_ne("Failed to download $1", package.name)
		return
	end
end


function contentdb.queue_download(package, reason)
	if package.queued or package.downloading then
		return
	end

	local max_concurrent_downloads = tonumber(core.settings:get("contentdb_max_concurrent_downloads"))
	if contentdb.number_downloading < math.max(max_concurrent_downloads, 1) then
		start_install(package, reason)
	else
		table.insert(contentdb.download_queue, { package = package, reason = reason })
		package.queued = true
	end
end


function contentdb.get_package_by_id(id)
	return contentdb.package_by_id[id]
end


local function strip_game_suffix(type, name)
	if (type == nil or type == "game") and #name > 5 and name:sub(#name - 4) == "_game" then
		return name:sub(1, #name - 5)
	else
		return name
	end
end

function contentdb.calculate_package_id(type, author, name)
	return author:lower() .. "/" .. strip_game_suffix(type, name)
end


function contentdb.get_package_by_info(author, name)
	local id = contentdb.calculate_package_id(nil, author, name)
	return contentdb.package_by_id[id]
end


-- Create a coroutine from `fn` and provide results to `callback` when complete (dead).
-- Returns a resumer function.
local function make_callback_coroutine(fn, callback)
	local co = coroutine.create(fn)

	local function resumer(...)
		local ok, result = coroutine.resume(co, ...)

		if not ok then
			error(result)
		elseif coroutine.status(co) == "dead" then
			callback(result)
		end
	end

	return resumer
end


local function get_raw_dependencies_async(package)
	local url_fmt = "/api/packages/%s/dependencies/?only_hard=1&protocol_version=%s&engine_version=%s"
	local version = core.get_version()
	local base_url = core.settings:get("contentdb_url")
	local url = base_url .. url_fmt:format(package.url_part, core.get_max_supp_proto(), core.urlencode(version.string))

	local http = core.get_http_api()
	local response = http.fetch_sync({ url = url })
	if not response.succeeded then
		return nil
	end
	return core.parse_json(response.data) or {}
end


local function get_raw_dependencies_co(package, resumer)
	if package.type ~= "mod" then
		return {}
	end
	if package.raw_deps then
		return package.raw_deps
	end

	core.handle_async(get_raw_dependencies_async, package, resumer)
	local data = coroutine.yield()
	if not data then
		return nil
	end

	for id, raw_deps in pairs(data) do
		local package2 = contentdb.package_by_id[id:lower()]
		if package2 and not package2.raw_deps then
			package2.raw_deps = raw_deps

			for _, dep in pairs(raw_deps) do
				local packages = {}
				for i=1, #dep.packages do
					packages[#packages + 1] = contentdb.package_by_id[dep.packages[i]:lower()]
				end
				dep.packages = packages
			end
		end
	end

	return package.raw_deps
end


local function has_hard_deps_co(package, resumer)
	local raw_deps = get_raw_dependencies_co(package, resumer)
	if not raw_deps then
		return nil
	end

	for i=1, #raw_deps do
		if not raw_deps[i].is_optional then
			return true
		end
	end

	return false
end


function contentdb.has_hard_deps(package, callback)
	local resumer = make_callback_coroutine(has_hard_deps_co, callback)
	resumer(package, resumer)
end


-- Recursively resolve dependencies, given the installed mods
local function resolve_dependencies_2_co(raw_deps, installed_mods, out, resumer)
	local function resolve_dep(dep)
		-- Check whether it's already installed
		if installed_mods[dep.name] then
			return {
				is_optional = dep.is_optional,
				name = dep.name,
				installed = true,
			}
		end

		-- Find exact name matches
		local fallback
		for _, package in pairs(dep.packages) do
			if package.type ~= "game" then
				if package.name == dep.name then
					return {
						is_optional = dep.is_optional,
						name = dep.name,
						installed = false,
						package = package,
					}
				elseif not fallback then
					fallback = package
				end
			end
		end

		-- Otherwise, find the first mod that fulfills it
		if fallback then
			return {
				is_optional = dep.is_optional,
				name = dep.name,
				installed = false,
				package = fallback,
			}
		end

		return {
			is_optional = dep.is_optional,
			name = dep.name,
			installed = false,
		}
	end

	for _, dep in pairs(raw_deps) do
		if not dep.is_optional and not out[dep.name] then
			local result  = resolve_dep(dep)
			out[dep.name] = result
			if result and result.package and not result.installed then
				local raw_deps2 = get_raw_dependencies_co(result.package, resumer)
				if raw_deps2 then
					resolve_dependencies_2_co(raw_deps2, installed_mods, out, resumer)
				end
			end
		end
	end

	return true
end


local function resolve_dependencies_co(package, game, resumer)
	assert(game)

	local raw_deps = get_raw_dependencies_co(package, resumer)
	local installed_mods = {}

	local mods = {}
	pkgmgr.get_game_mods(game, mods)
	for _, mod in pairs(mods) do
		installed_mods[mod.name] = true
	end

	for _, mod in pairs(pkgmgr.global_mods:get_list()) do
		installed_mods[mod.name] = true
	end

	local out = {}
	if not resolve_dependencies_2_co(raw_deps, installed_mods, out, resumer) then
		return nil
	end

	local retval = {}
	for _, dep in pairs(out) do
		retval[#retval + 1] = dep
	end

	table.sort(retval, function(a, b)
		return a.name < b.name
	end)

	return retval
end


-- Resolve dependencies for a package, calls the recursive version.
function contentdb.resolve_dependencies(package, game, callback)
	local resumer = make_callback_coroutine(resolve_dependencies_co, callback)
	resumer(package, game, resumer)
end


local function fetch_pkgs()
	local version = core.get_version()
	local base_url = core.settings:get("contentdb_url")
	local url = base_url ..
			"/api/packages/?type=mod&type=game&type=txp&protocol_version=" ..
			core.get_max_supp_proto() .. "&engine_version=" .. core.urlencode(version.string)
	for _, item in pairs(core.settings:get("contentdb_flag_blacklist"):split(",")) do
		item = item:trim()
		if item ~= "" then
			url = url .. "&hide=" .. core.urlencode(item)
		end
	end

	local http = core.get_http_api()
	local response = http.fetch_sync({
		url = url,
		extra_headers = {
			core.get_http_accept_languages()
		},
	})
	if not response.succeeded then
		return
	end

	local packages = core.parse_json(response.data)
	if not packages or #packages == 0 then
		return
	end
	return packages
end


function contentdb.set_packages_from_api(packages)
	contentdb.package_by_id = {}
	contentdb.aliases = {}

	for _, package in pairs(packages) do
		package.id = contentdb.calculate_package_id(package.type, package.author, package.name)
		package.url_part = core.urlencode(package.author) .. "/" .. core.urlencode(package.name)

		contentdb.package_by_id[package.id] = package

		if package.aliases then
			for _, alias in ipairs(package.aliases) do
				-- We currently don't support name changing
				local suffix = "/" .. package.name
				if alias:sub(-#suffix) == suffix then
					contentdb.aliases[strip_game_suffix(packages.type, alias:lower())] = package.id
				end
			end
		end
	end

	contentdb.load_ok = true
	contentdb.load_error = false
	contentdb.packages = packages
	contentdb.packages_full = packages
	contentdb.packages_full_unordered = packages
end

function contentdb.fetch_pkgs(callback)
	contentdb.loading = true
	core.handle_async(fetch_pkgs, nil, function(result)
		if result then
			contentdb.set_packages_from_api(result)
		else
			contentdb.load_error = true
		end

		contentdb.loading = false
		callback(result)
	end)
end


function contentdb.update_paths()
	pkgmgr.load_all()

	local mod_hash = {}
	for _, mod in pairs(pkgmgr.global_mods:get_list()) do
		local cdb_id = pkgmgr.get_contentdb_id(mod)
		if cdb_id then
			mod_hash[contentdb.aliases[cdb_id] or cdb_id] = mod
		end
	end

	local game_hash = {}
	for _, game in pairs(pkgmgr.games) do
		local cdb_id = pkgmgr.get_contentdb_id(game)
		if cdb_id then
			game_hash[contentdb.aliases[cdb_id] or cdb_id] = game
		end
	end

	local txp_hash = {}
	for _, txp in pairs(pkgmgr.texture_packs) do
		local cdb_id = pkgmgr.get_contentdb_id(txp)
		if cdb_id then
			txp_hash[contentdb.aliases[cdb_id] or cdb_id] = txp
		end
	end

	for _, package in pairs(contentdb.packages_full) do
		local content
		if package.type == "mod" then
			content = mod_hash[package.id]
		elseif package.type == "game" then
			content = game_hash[package.id]
		elseif package.type == "txp" then
			content = txp_hash[package.id]
		end

		if content then
			package.path = content.path
			package.installed_release = content.release or 0
		else
			package.path = nil
			package.installed_release = nil
		end
	end
end


function contentdb.sort_packages()
	local ret = {}

	-- Add installed content
	for _, pkg in ipairs(contentdb.packages_full_unordered) do
		if pkg.path then
			ret[#ret + 1] = pkg
		end
	end

	-- Sort installed content first by "is there an update available?", then by title
	table.sort(ret, function(a, b)
		local a_updatable = a.installed_release < a.release
		local b_updatable = b.installed_release < b.release
		if a_updatable and not b_updatable then
			return true
		elseif b_updatable and not a_updatable then
			return false
		end

		return a.title < b.title
	end)

	-- Add uninstalled content
	for _, pkg in ipairs(contentdb.packages_full_unordered) do
		if not pkg.path then
			ret[#ret + 1] = pkg
		end
	end

	contentdb.packages_full = ret
end


function contentdb.filter_packages(query, by_type)
	if query == "" and by_type == nil then
		contentdb.packages = contentdb.packages_full
		return
	end

	local keywords = {}
	for word in query:gmatch("%S+") do
		table.insert(keywords, word:lower())
	end

	local function contains_all_keywords(str)
		str = str:lower()
		for _, keyword in ipairs(keywords) do
			if not str:find(keyword, 1, true) then
				return false
			end
		end
		return true
	end

	local function matches_keywords(package)
		return contains_all_keywords(package.name) or
				contains_all_keywords(package.title) or
				contains_all_keywords(package.author) or
				contains_all_keywords(package.short_description)
	end

	contentdb.packages = {}
	for _, package in pairs(contentdb.packages_full) do
		if (query == "" or matches_keywords(package)) and
				(by_type == nil or package.type == by_type) then
			table.insert(contentdb.packages, package)
		end
	end
end


local function get_package_info(key, path)
	return function(package, callback)
		assert(package)
		if package[key] then
			callback(package[key])
			return
		end

		local function fetch(params)
			local version = core.get_version()
			local base_url = core.settings:get("contentdb_url")
			local url = base_url ..
					"/api/packages/" .. params.package.url_part .. params.path .. "?" ..
					"protocol_version=" .. core.urlencode(core.get_max_supp_proto()) ..
					"&engine_version=" .. core.urlencode(version.string) ..
					"&formspec_version=" .. core.urlencode(core.get_formspec_version()) ..
					"&include_images=false"
			local http = core.get_http_api()
			local response = http.fetch_sync({
				url = url,
				extra_headers = {
					core.get_http_accept_languages()
				},
			})
			if not response.succeeded then
				return nil
			end

			return core.parse_json(response.data)
		end

		local function my_callback(value)
			package[key] = value
			callback(value)
		end

		if not core.handle_async(fetch, { package = package, path = path }, my_callback) then
			core.log("error", "ERROR: async event failed")
			callback(nil)
		end
	end
end


contentdb.get_full_package_info = get_package_info("full_info", "/for-client/")
contentdb.get_package_reviews = get_package_info("reviews", "/for-client/reviews/")


function contentdb.get_formspec_padding()
	-- Padding is increased on Android to account for notches
	-- TODO: use Android API to determine size of cut outs
	return { x = PLATFORM == "Android" and 1 or 0.5, y = PLATFORM == "Android" and 0.25 or 0.5 }
end


function contentdb.get_formspec_size()
	local window = core.get_window_info()
	local size = { x = window.max_formspec_size.x, y = window.max_formspec_size.y }

	-- Minimum formspec size
	local min_x = 15.5
	local min_y = 10
	if size.x < min_x or size.y < min_y then
		local scale = math.max(min_x / size.x, min_y / size.y)
		size.x = size.x * scale
		size.y = size.y * scale
	end

	return size
end
