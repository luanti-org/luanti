-- Luanti
-- Copyright (C) 2020 rubenwardy
-- SPDX-License-Identifier: LGPL-2.1-or-later

serverlistmgr = {
	-- continent code we detected for ourselves
	my_continent = nil,

	-- list of locally favorites servers
	favorites = nil,

	-- list of servers fetched from public list
	servers = nil,
}

do
	if check_cache_age("geoip_last_checked", 3600) then
		local tmp = cache_settings:get("geoip") or ""
		if tmp:match("^[A-Z][A-Z]$") then
			serverlistmgr.my_continent = tmp
		end
	end
end

--------------------------------------------------------------------------------
-- Efficient data structure for normalizing arbitrary scores attached to objects
-- e.g. {{"a", 3.14}, {"b", 3.14}, {"c", 20}, {"d", 0}}
--   -> {["d"] = 0, ["a"] = 0.5, ["b"] = 0.5, ["c"] = 1}
local Normalizer = {}

function Normalizer:new()
	local t = {
		map = {}
	}
	setmetatable(t, self)
	self.__index = self
	return t
end

function Normalizer:push(obj, score)
	if not self.map[score] then
		self.map[score] = {}
	end
	local t = self.map[score]
	t[#t + 1] = obj
end

function Normalizer:calc()
	local list = {}
	for k, _ in pairs(self.map) do
		list[#list + 1] = k
	end
	table.sort(list)
	local ret = {}
	for i, k in ipairs(list) do
		local score = #list == 1 and 1 or ( (i - 1) / (#list - 1) )
		for _, obj in ipairs(self.map[k]) do
			ret[obj] = score
		end
	end
	return ret
end

--------------------------------------------------------------------------------
-- how much the pre-sorted server list contributes to the final ranking
local WEIGHT_SORT = 2
-- how much the estimated latency contributes to the final ranking
local WEIGHT_LATENCY = 1

--- @param list of servers, will be modified.
local function order_server_list_internal(list)
	-- calculate the scores
	local s1 = Normalizer:new()
	local s2 = Normalizer:new()
	for i, fav in ipairs(list) do
		-- first: the original position
		s1:push(fav, #list - i)
		-- second: estimated latency
		local ping = (fav.ping or 0) * 1000
		if ping < 400 then
			-- If ping is under 400ms replace it with our own estimate,
			-- we assume the server has latency issues anyway otherwise
			ping = estimate_continent_latency(serverlistmgr.my_continent, fav) or 0
		end
		s2:push(fav, -ping)
	end
	s1 = s1:calc()
	s2 = s2:calc()

	-- pre-calculate ordering
	local order = {}
	for _, fav in ipairs(list) do
		order[fav] = s1[fav] * WEIGHT_SORT + s2[fav] * WEIGHT_LATENCY
	end

	-- now sort the list
	table.sort(list, function(fav1, fav2)
		return order[fav1] > order[fav2]
	end)
end

local function order_server_list(list)
	-- split the list into two parts and sort them separately, to keep empty
	-- servers at the bottom.
	local nonempty, empty = {}, {}

	for _, fav in ipairs(list) do
		if (fav.clients or 0) > 0 then
			table.insert(nonempty, fav)
		else
			table.insert(empty, fav)
		end
	end

	order_server_list_internal(nonempty)
	order_server_list_internal(empty)

	table.insert_all(nonempty, empty)
	return nonempty
end

local public_downloading = false
local geoip_downloading = false

--------------------------------------------------------------------------------
local function fetch_geoip()
	local http = core.get_http_api()
	local url = core.settings:get("serverlist_url") .. "/geoip"

	local response = http.fetch_sync({ url = url })
	if not response.succeeded then
		return
	end

	local retval = core.parse_json(response.data)
	if type(retval) ~= "table" then
		return
	end
	return type(retval.continent) == "string" and retval.continent
end

function serverlistmgr.sync()
	if not serverlistmgr.servers then
		serverlistmgr.servers = {{
			name = fgettext("Loading..."),
			description = fgettext_ne("Try reenabling public serverlist and check your internet connection.")
		}}
	end

	local serverlist_url = core.settings:get("serverlist_url") or ""
	if not core.get_http_api or serverlist_url == "" then
		serverlistmgr.servers = {{
			name = fgettext("Public server list is disabled"),
			description = ""
		}}
		return
	end

	if not serverlistmgr.my_continent and not geoip_downloading then
		geoip_downloading = true
		core.handle_async(fetch_geoip, nil, function(result)
			geoip_downloading = false
			if not result then
				return
			end
			serverlistmgr.my_continent = result
			cache_settings:set("geoip", result)
			cache_settings:set("geoip_last_checked", tostring(os.time()))

			-- re-sort list if applicable
			if serverlistmgr.servers then
				serverlistmgr.servers = order_server_list(serverlistmgr.servers)
				core.event_handler("Refresh")
			end
		end)
	end

	if public_downloading then
		return
	end
	public_downloading = true

	-- note: this isn't cached because it's way too dynamic
	core.handle_async(
		function(param)
			local http = core.get_http_api()
			local url = ("%s/list?proto_version_min=%d&proto_version_max=%d"):format(
				core.settings:get("serverlist_url"),
				core.get_min_supp_proto(),
				core.get_max_supp_proto())

			local response = http.fetch_sync({ url = url })
			if not response.succeeded then
				return {}
			end

			local retval = core.parse_json(response.data)
			return retval and retval.list or {}
		end,
		nil,
		function(result)
			public_downloading = false
			local favs = order_server_list(result)
			if favs[1] then
				serverlistmgr.servers = favs
			end
			core.event_handler("Refresh")
		end
	)
end

--------------------------------------------------------------------------------
local function get_favorites_path(folder)
	local base = core.get_user_path() .. DIR_DELIM .. "client" .. DIR_DELIM .. "serverlist" .. DIR_DELIM
	if folder then
		return base
	end
	return base .. core.settings:get("serverlist_file")
end

--------------------------------------------------------------------------------
local function save_favorites(favorites)
	local filename = core.settings:get("serverlist_file")
	-- If setting specifies legacy format change the filename to the new one
	if filename:sub(#filename - 3):lower() == ".txt" then
		core.settings:set("serverlist_file", filename:sub(1, #filename - 4) .. ".json")
	end

	assert(core.create_dir(get_favorites_path(true)))
	core.safe_file_write(get_favorites_path(), core.write_json(favorites))
end

--------------------------------------------------------------------------------
function serverlistmgr.read_legacy_favorites(path)
	local file = io.open(path, "r")
	if not file then
		return nil
	end

	local lines = {}
	for line in file:lines() do
		lines[#lines + 1] = line
	end
	file:close()

	local favorites = {}

	local i = 1
	while i < #lines do
		local function pop()
			local line = lines[i]
			i = i + 1
			return line and line:trim()
		end

		if pop():lower() == "[server]" then
			local name = pop()
			local address = pop()
			local port = tonumber(pop())
			local description = pop()

			if name == "" then
				name = nil
			end

			if description == "" then
				description = nil
			end

			if not address or #address < 3 then
				core.log("warning", "Malformed favorites file, missing address at line " .. i)
			elseif not port or port < 1 or port > 65535 then
				core.log("warning", "Malformed favorites file, missing port at line " .. i)
			elseif (name and name:upper() == "[SERVER]") or
					(address and address:upper() == "[SERVER]") or
					(description and description:upper() == "[SERVER]") then
				core.log("warning", "Potentially malformed favorites file, overran at line " .. i)
			else
				favorites[#favorites + 1] = {
					name = name,
					address = address,
					port = port,
					description = description
				}
			end
		end
	end

	return favorites
end

--------------------------------------------------------------------------------
local function read_favorites()
	local path = get_favorites_path()

	-- If new format configured fall back to reading the legacy file
	if path:sub(#path - 4):lower() == ".json" then
		local file = io.open(path, "r")
		if file then
			local json = file:read("*all")
			file:close()
			return core.parse_json(json)
		end

		path = path:sub(1, #path - 5) .. ".txt"
	end

	local favs = serverlistmgr.read_legacy_favorites(path)
	if favs then
		save_favorites(favs)
		os.remove(path)
	end
	return favs
end

--------------------------------------------------------------------------------
local function delete_favorite(favorites, del_favorite)
	for i=1, #favorites do
		local fav = favorites[i]

		if fav.address == del_favorite.address and fav.port == del_favorite.port then
			table.remove(favorites, i)
			return
		end
	end
end

--------------------------------------------------------------------------------
function serverlistmgr.get_favorites()
	if serverlistmgr.favorites then
		return serverlistmgr.favorites
	end

	serverlistmgr.favorites = {}

	-- Add favorites, removing duplicates
	local seen = {}
	for _, fav in ipairs(read_favorites() or {}) do
		local key = ("%s:%d"):format(fav.address:lower(), fav.port)
		if not seen[key] then
			seen[key] = true
			serverlistmgr.favorites[#serverlistmgr.favorites + 1] = fav
		end
	end

	return serverlistmgr.favorites
end

--------------------------------------------------------------------------------
function serverlistmgr.add_favorite(new_favorite)
	assert(type(new_favorite.port) == "number")

	-- Whitelist favorite keys
	new_favorite = {
		name = new_favorite.name,
		address = new_favorite.address,
		port = new_favorite.port,
		description = new_favorite.description,
	}

	local favorites = serverlistmgr.get_favorites()
	delete_favorite(favorites, new_favorite)
	table.insert(favorites, 1, new_favorite)
	save_favorites(favorites)
end

--------------------------------------------------------------------------------
function serverlistmgr.delete_favorite(del_favorite)
	local favorites = serverlistmgr.get_favorites()
	delete_favorite(favorites, del_favorite)
	save_favorites(favorites)
end
