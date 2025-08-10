-- Luanti
-- Copyright (C) 2014 sapier
-- SPDX-License-Identifier: LGPL-2.1-or-later

-- Global menu data
menudata = {}

-- located in user cache path, for remembering this like e.g. last update check
cache_settings = Settings(core.get_cache_path() .. DIR_DELIM .. "common.conf")

--- Checks if the given key contains a timestamp less than a certain age.
--- Pair this with a call to `cache_settings:set(key, tostring(os.time()))`
--- after successfully refreshing the cache.
--- @param key Name of entry in cache_settings
--- @param max_age Age to check against, in seconds
--- @return true if the max age is not reached
function check_cache_age(key, max_age)
	local time_now = os.time()
	local time_checked = tonumber(cache_settings:get(key)) or 0
	return time_now - time_checked < max_age
end

function core.on_before_close()
	cache_settings:write()
end

-- Local cached values
local min_supp_proto, max_supp_proto

function common_update_cached_supp_proto()
	min_supp_proto = core.get_min_supp_proto()
	max_supp_proto = core.get_max_supp_proto()
end
common_update_cached_supp_proto()

-- Other global functions

function core.sound_stop(handle, ...)
	return handle:stop(...)
end

function os.tmpname()
	error('do not use') -- instead: core.get_temp_path()
end

-- Menu helper functions

local function render_client_count(n)
	if     n > 999 then return '99+'
	elseif n >= 0  then return tostring(n)
	else return '?' end
end

local function configure_selected_world_params(idx)
	local worldconfig = pkgmgr.get_worldconfig(menudata.worldlist:get_list()[idx].path)
	if worldconfig.creative_mode then
		core.settings:set("creative_mode", worldconfig.creative_mode)
	end
	if worldconfig.enable_damage then
		core.settings:set("enable_damage", worldconfig.enable_damage)
	end
end

-- retrieved from https://wondernetwork.com/pings with (hopefully) representative cities
-- Amsterdam, Auckland, Brasilia, Denver, Lagos, Singapore
local latency_matrix = {
	["AF"] = { ["AS"]=258, ["EU"]=100, ["NA"]=218, ["OC"]=432, ["SA"]=308 },
	["AS"] = { ["EU"]=168, ["NA"]=215, ["OC"]=125, ["SA"]=366 },
	["EU"] = { ["NA"]=120, ["OC"]=298, ["SA"]=221 },
	["NA"] = { ["OC"]=202, ["SA"]=168 },
	["OC"] = { ["SA"]=411 },
	["SA"] = {}
}
function estimate_continent_latency(own, spec)
	local there = spec.geo_continent
	if not own or not there then
		return nil
	end
	if own == there then
		return 0
	end
	return latency_matrix[there][own] or latency_matrix[own][there]
end

function render_serverlist_row(spec)
	local text = ""
	if spec.name then
		text = text .. core.formspec_escape(spec.name:trim())
	elseif spec.address then
		text = text .. core.formspec_escape(spec.address:trim())
		if spec.port then
			text = text .. ":" .. spec.port
		end
	end

	local grey_out = not spec.is_compatible

	local details = {}

	if spec.lag or spec.ping then
		local lag = (spec.lag or 0) * 1000 + (spec.ping or 0) * 250
		if lag <= 125 then
			table.insert(details, "1")
		elseif lag <= 175 then
			table.insert(details, "2")
		elseif lag <= 250 then
			table.insert(details, "3")
		else
			table.insert(details, "4")
		end
	else
		table.insert(details, "0")
	end

	table.insert(details, ",")

	local color = (grey_out and "#aaaaaa") or ((spec.is_favorite and "#ddddaa") or "#ffffff")
	if spec.clients and (spec.clients_max or 0) > 0 then
		local clients_percent = 100 * spec.clients / spec.clients_max

		-- Choose a color depending on how many clients are connected
		-- (relatively to clients_max)
		local clients_color
		if     grey_out		      then clients_color = '#aaaaaa'
		elseif spec.clients == 0      then clients_color = ''        -- 0 players: default/white
		elseif clients_percent <= 60  then clients_color = '#a1e587' -- 0-60%: green
		elseif clients_percent <= 90  then clients_color = '#ffdc97' -- 60-90%: yellow
		elseif clients_percent == 100 then clients_color = '#dd5b5b' -- full server: red (darker)
		else                               clients_color = '#ffba97' -- 90-100%: orange
		end

		table.insert(details, clients_color)
		table.insert(details, render_client_count(spec.clients) .. " / " ..
			render_client_count(spec.clients_max))
	else
		table.insert(details, color)
		table.insert(details, "?")
	end

	if spec.creative then
		table.insert(details, "1") -- creative icon
	else
		table.insert(details, "0")
	end

	if spec.pvp then
		table.insert(details, "2") -- pvp icon
	elseif spec.damage then
		table.insert(details, "1") -- heart icon
	else
		table.insert(details, "0")
	end

	table.insert(details, color)
	table.insert(details, text)

	return table.concat(details, ",")
end

function menu_render_worldlist()
	local retval = {}
	local current_worldlist = menudata.worldlist:get_list()

	for i, v in ipairs(current_worldlist) do
		retval[#retval+1] = core.formspec_escape(v.name)
	end

	return table.concat(retval, ",")
end

function menu_handle_key_up_down(fields, textlist, settingname)
	local oldidx, newidx = core.get_textlist_index(textlist), 1
	if fields.key_up or fields.key_down then
		if fields.key_up and oldidx and oldidx > 1 then
			newidx = oldidx - 1
		elseif fields.key_down and oldidx and
				oldidx < menudata.worldlist:size() then
			newidx = oldidx + 1
		end
		core.settings:set(settingname, menudata.worldlist:get_raw_index(newidx))
		configure_selected_world_params(newidx)
		return true
	end
	return false
end

function text2textlist(xpos, ypos, width, height, tl_name, textlen, text, transparency)
	local textlines = core.wrap_text(text, textlen, true)
	local retval = "textlist[" .. xpos .. "," .. ypos .. ";" .. width ..
			"," .. height .. ";" .. tl_name .. ";"

	for i = 1, #textlines do
		textlines[i] = textlines[i]:gsub("\r", "")
		retval = retval .. core.formspec_escape(textlines[i]) .. ","
	end

	retval = retval .. ";0;"
	if transparency then retval = retval .. "true" end
	retval = retval .. "]"

	return retval
end

function is_server_protocol_compat(server_proto_min, server_proto_max)
	if (not server_proto_min) or (not server_proto_max) then
		-- There is no info. Assume the best and act as if we would be compatible.
		return true
	end
	return min_supp_proto <= server_proto_max and max_supp_proto >= server_proto_min
end

function is_server_protocol_compat_or_error(server_proto_min, server_proto_max)
	if not is_server_protocol_compat(server_proto_min, server_proto_max) then
		local server_prot_ver_info, client_prot_ver_info
		local s_p_min = server_proto_min
		local s_p_max = server_proto_max

		if s_p_min ~= s_p_max then
			server_prot_ver_info = fgettext_ne("Server supports protocol versions between $1 and $2. ",
				s_p_min, s_p_max)
		else
			server_prot_ver_info = fgettext_ne("Server enforces protocol version $1. ",
				s_p_min)
		end
		if min_supp_proto ~= max_supp_proto then
			client_prot_ver_info= fgettext_ne("We support protocol versions between version $1 and $2.",
				min_supp_proto, max_supp_proto)
		else
			client_prot_ver_info = fgettext_ne("We only support protocol version $1.", min_supp_proto)
		end
		gamedata.errormessage = fgettext_ne("Protocol version mismatch. ")
			.. server_prot_ver_info
			.. client_prot_ver_info
		return false
	end

	return true
end

function menu_worldmt(selected, setting, value)
	local world = menudata.worldlist:get_list()[selected]
	if world then
		local filename = world.path .. DIR_DELIM .. "world.mt"
		local world_conf = Settings(filename)

		if value then
			if not world_conf:write() then
				core.log("error", "Failed to write world config file")
			end
			world_conf:set(setting, value)
			world_conf:write()
		else
			return world_conf:get(setting)
		end
	else
		return nil
	end
end

function menu_worldmt_legacy(selected)
	local modes_names = {"creative_mode", "enable_damage", "server_announce"}
	for _, mode_name in pairs(modes_names) do
		local mode_val = menu_worldmt(selected, mode_name)
		if mode_val then
			core.settings:set(mode_name, mode_val)
		else
			menu_worldmt(selected, mode_name, core.settings:get(mode_name))
		end
	end
end

function confirmation_formspec(message, confirm_id, confirm_label, cancel_id, cancel_label)
	return "size[10,2.5,true]" ..
			"label[0.5,0.5;" .. message .. "]" ..
			"style[" .. confirm_id .. ";bgcolor=red]" ..
			"button[0.5,1.5;2.5,0.5;" .. confirm_id .. ";" .. confirm_label .. "]" ..
			"button[7.0,1.5;2.5,0.5;" .. cancel_id .. ";" .. cancel_label .. "]"
end
