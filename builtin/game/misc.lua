local S = core.get_translator("__builtin")

--
-- Misc. API functions
--

-- @spec core.kick_player(String, String, Boolean) :: Boolean
function core.kick_player(player_name, reason, reconnect)
	if type(reason) == "string" then
		reason = "Kicked: " .. reason
	else
		reason = "Kicked."
	end
	return core.disconnect_player(player_name, reason, reconnect)
end

function core.check_player_privs(name, ...)
	if core.is_player(name) then
		name = name:get_player_name()
	elseif type(name) ~= "string" then
		error("core.check_player_privs expects a player or playername as " ..
			"argument.", 2)
	end

	local requested_privs = {...}
	local player_privs = core.get_player_privs(name)
	local missing_privileges = {}

	if type(requested_privs[1]) == "table" then
		-- We were provided with a table like { privA = true, privB = true }.
		for priv, value in pairs(requested_privs[1]) do
			if value and not player_privs[priv] then
				missing_privileges[#missing_privileges + 1] = priv
			end
		end
	else
		-- Only a list, we can process it directly.
		for key, priv in pairs(requested_privs) do
			if not player_privs[priv] then
				missing_privileges[#missing_privileges + 1] = priv
			end
		end
	end

	if #missing_privileges > 0 then
		return false, missing_privileges
	end

	return true, ""
end


function core.send_join_message(player_name)
	if not core.is_singleplayer() then
		core.chat_send_all("*** " .. S("@1 joined the game.", player_name))
	end
end


function core.send_leave_message(player_name, timed_out)
	local announcement = "*** " .. S("@1 left the game.", player_name)
	if timed_out then
		announcement = "*** " .. S("@1 left the game (timed out).", player_name)
	end
	core.chat_send_all(announcement)
end


core.register_on_joinplayer(function(player)
	local player_name = player:get_player_name()
	if not core.is_singleplayer() then
		local status = core.get_server_status(player_name, true)
		if status and status ~= "" then
			core.chat_send_player(player_name, status)
		end
	end
	core.send_join_message(player_name)
end)


core.register_on_leaveplayer(function(player, timed_out)
	local player_name = player:get_player_name()
	core.send_leave_message(player_name, timed_out)
end)


function core.is_player(player)
	-- a table being a player is also supported because it quacks sufficiently
	-- like a player if it has the is_player function
	local t = type(player)
	return (t == "userdata" or t == "table") and
		type(player.is_player) == "function" and player:is_player()
end


function core.player_exists(name)
	return core.get_auth_handler().get_auth(name) ~= nil
end


-- Returns two position vectors representing a box of `radius` in each
-- direction centered around the player corresponding to `player_name`

function core.get_player_radius_area(player_name, radius)
	local player = core.get_player_by_name(player_name)
	if player == nil then
		return nil
	end

	local p1 = player:get_pos()
	local p2 = p1

	if radius then
		p1 = vector.subtract(p1, radius)
		p2 = vector.add(p2, radius)
	end

	return p1, p2
end


-- To be overridden by protection mods

function core.is_protected(pos, name)
	return false
end


function core.record_protection_violation(pos, name)
	for _, func in pairs(core.registered_on_protection_violation) do
		func(pos, name)
	end
end

-- To be overridden by Creative mods

local creative_mode_cache = core.settings:get_bool("creative_mode")
function core.is_creative_enabled(name)
	return creative_mode_cache
end

-- Checks if specified volume intersects a protected volume

function core.is_area_protected(minp, maxp, player_name, interval)
	-- 'interval' is the largest allowed interval for the 3D lattice of checks.

	-- Compute the optimal float step 'd' for each axis so that all corners and
	-- borders are checked. 'd' will be smaller or equal to 'interval'.
	-- Subtracting 1e-4 ensures that the max co-ordinate will be reached by the
	-- for loop (which might otherwise not be the case due to rounding errors).

	-- Default to 4
	interval = interval or 4
	local d = {}

	for _, c in pairs({"x", "y", "z"}) do
		if minp[c] > maxp[c] then
			-- Repair positions: 'minp' > 'maxp'
			local tmp = maxp[c]
			maxp[c] = minp[c]
			minp[c] = tmp
		end

		if maxp[c] > minp[c] then
			d[c] = (maxp[c] - minp[c]) /
				math.ceil((maxp[c] - minp[c]) / interval) - 1e-4
		else
			d[c] = 1 -- Any value larger than 0 to avoid division by zero
		end
	end

	for zf = minp.z, maxp.z, d.z do
		local z = math.floor(zf + 0.5)
		for yf = minp.y, maxp.y, d.y do
			local y = math.floor(yf + 0.5)
			for xf = minp.x, maxp.x, d.x do
				local x = math.floor(xf + 0.5)
				local pos = vector.new(x, y, z)
				if core.is_protected(pos, player_name) then
					return pos
				end
			end
		end
	end
	return false
end


local raillike_ids = {}
local raillike_cur_id = 0
function core.raillike_group(name)
	local id = raillike_ids[name]
	if not id then
		raillike_cur_id = raillike_cur_id + 1
		raillike_ids[name] = raillike_cur_id
		id = raillike_cur_id
	end
	return id
end


-- HTTP callback interface

core.set_http_api_lua(function(httpenv)
	httpenv.fetch = function(req, callback)
		local handle = httpenv.fetch_async(req)

		local function update_http_status()
			local res = httpenv.fetch_async_get(handle)
			if res.completed then
				callback(res)
			else
				core.after(0, update_http_status)
			end
		end
		core.after(0, update_http_status)
	end

	return httpenv
end)
core.set_http_api_lua = nil


function core.close_formspec(player_name, formname)
	return core.show_formspec(player_name, formname, "")
end


function core.cancel_shutdown_requests()
	core.request_shutdown("", false, -1)
end


-- Used for callback handling with dynamic_add_media
core.dynamic_media_callbacks = {}


-- Transfer of certain globals into seconday Lua environments
-- see builtin/async/game.lua or builtin/emerge/register.lua for the unpacking

local function copy_filtering(t, seen)
	if type(t) == "userdata" or type(t) == "function" then
		return true -- don't use nil so presence can still be detected
	elseif type(t) ~= "table" then
		return t
	end
	local n = {}
	seen = seen or {}
	seen[t] = n
	for k, v in pairs(t) do
		local k_ = seen[k] or copy_filtering(k, seen)
		local v_ = seen[v] or copy_filtering(v, seen)
		n[k_] = v_
	end
	return n
end

function core.get_globals_to_transfer()
	local all = {
		registered_items = copy_filtering(core.registered_items),
		registered_aliases = core.registered_aliases,
		registered_biomes = core.registered_biomes,
		registered_ores = core.registered_ores,
		registered_decorations = core.registered_decorations,

		nodedef_default = copy_filtering(core.nodedef_default),
		craftitemdef_default = copy_filtering(core.craftitemdef_default),
		tooldef_default = copy_filtering(core.tooldef_default),
		noneitemdef_default = copy_filtering(core.noneitemdef_default),
	}
	return all
end

do
	local function valid_object_iterator(objects)
		local i = 0
		local function next_valid_object()
			i = i + 1
			local obj = objects[i]
			if obj == nil then
				return
			end
			if obj:is_valid() then
				return obj
			end
			return next_valid_object()
		end
		return next_valid_object
	end

	function core.objects_inside_radius(center, radius)
		return valid_object_iterator(core.get_objects_inside_radius(center, radius))
	end

	function core.objects_in_area(min_pos, max_pos)
		return valid_object_iterator(core.get_objects_in_area(min_pos, max_pos))
	end
end

--
-- Helper for LBM execution, called from C++
--

function core.run_lbm(id, pos_list, dtime_s)
	local lbm = core.registered_lbms[id]
	assert(lbm, "Entry with given id not found in registered_lbms table")
	core.set_last_run_mod(lbm.mod_origin)
	if lbm.bulk_action then
		return lbm.bulk_action(pos_list, dtime_s)
	end
	-- emulate non-bulk LBMs
	local expect = core.get_node(pos_list[1]).name
	-- engine guarantees that
	-- 1) all nodes are the same content type
	-- 2) the list is up-to-date when we're called
	assert(expect ~= "ignore")
	for _, pos in ipairs(pos_list) do
		local n = core.get_node(pos)
		if n.name == expect then -- might have been changed by previous call
			lbm.action(pos, n, dtime_s)
		end
	end
end
