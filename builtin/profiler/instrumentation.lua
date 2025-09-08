-- Luanti
-- Copyright (C) 2016 T4im
-- SPDX-License-Identifier: LGPL-2.1-or-later

local format, ipairs, type = string.format, ipairs, type
local core, get_current_modname = core, core.get_current_modname
local profiler, sampler = ...

local instrument_builtin = core.settings:get_bool("instrument.builtin", false)

local do_measure = core.settings:get_bool("profiler.measure", true)
local do_tracy = core.settings:get_bool("profiler.tracy", false)
if do_tracy and not core.global_exists("tracy") then
	core.log("warning", "profiler.tracy is enabled, but `tracy` was not found. Did you build with Tracy?")
	do_tracy = false
end

-- keep in sync with game/register.lua
local register_functions = {
	"register_on_player_hpchange",

	"register_on_chat_message",
	"register_on_chatcommand",
	"register_globalstep",
	"register_playerevent",
	"register_on_mods_loaded",
	"register_on_shutdown",
	"register_on_punchnode",
	"register_on_placenode",
	"register_on_dignode",
	"register_on_generated",
	"register_on_newplayer",
	"register_on_dieplayer",
	"register_on_respawnplayer",
	"register_on_prejoinplayer",
	"register_on_joinplayer",
	"register_on_leaveplayer",
	"register_on_player_receive_fields",
	"register_on_cheat",
	"register_on_craft",
	"register_craft_predict",
	"register_on_protection_violation",
	"register_on_item_eat",
	"register_on_item_pickup",
	"register_on_punchplayer",
	"register_on_priv_grant",
	"register_on_priv_revoke",
	"register_on_authplayer",
	"register_can_bypass_userlimit",
	"register_on_modchannel_message",
	"register_on_player_inventory_action",
	"register_allow_player_inventory_action",
	"register_on_rightclickplayer",
	"register_on_liquid_transformed",
	"register_on_mapblocks_changed",
}

local function regex_escape(s)
	return s:gsub("(%W)", "%%%1")
end

---
-- Format `filepath#linenumber` of function, with a relative filepath.
--
-- FIXME: these paths are not canonicalized (i.e. can be .../luanti/bin/..)
local worldmods_path = regex_escape(core.get_worldpath())
local user_path = regex_escape(core.get_user_path())
local builtin_path = regex_escape(core.get_builtin_path())
local function generate_source_location(def)
	local info = debug.getinfo(def.func)
	local modpath = regex_escape(core.get_modpath(def.mod) or "")
	local source = info.source
	if modpath ~= "" then
		source = source:gsub(modpath, def.mod)
	end
	source = source:gsub(worldmods_path, "")
	source = source:gsub(builtin_path, "builtin" .. DIR_DELIM)
	source = source:gsub(user_path, "")
	return string.format("%s#%s", source, info.linedefined)
end

---
-- Create an unique instrument name.
-- Generate a missing label with a running index number.
-- Returns name, name_has_source.
--
local generate_name_counts = {}
local function generate_name(def)
	local class, label, func_name = def.class, def.label, def.func_name
	local source = generate_source_location(def)

	if label then
		if class or func_name then
			return format("%s '%s' %s", class or "", label, func_name or ""):trim(), false
		end
		return format("%s", label):trim(), false
	elseif label == false then
		return format("%s", class or func_name):trim(), false
	else
		local index_id = def.mod .. (class or func_name)
		local index = generate_name_counts[index_id] or 1
		generate_name_counts[index_id] = index + 1
		return format("%s[%d] %s", class or func_name, index, source), true
	end
end

---
-- Keep `measure` and the closure in `instrument` lean, as these, and their
-- directly called functions are the overhead that is caused by instrumentation.
--
local time, log = core.get_us_time, sampler.log
local function measure(modname, instrument_name, start, ...)
	log(modname, instrument_name, time() - start)
	return ...
end
local function tracy_ZoneEnd_and_return(...)
	tracy.ZoneEnd()
	return ...
end
--- Automatically instrument a function to measure and log to the sampler.
-- def = {
-- 		mod = "",
-- 		class = "",
-- 		func_name = "",
-- 		-- if nil, will create a label based on registration order
-- 		label = "" | false,
-- }
local function instrument(def)
	if not def or not def.func then
		return
	end
	def.mod = def.mod or get_current_modname() or "??"
	local modname = def.mod
	local instrument_name, name_has_source = generate_name(def)
	local instrument_name_with_source = name_has_source and instrument_name or
		string.format("%s %s", instrument_name, generate_source_location(def))
	local func = def.func

	if not instrument_builtin and modname == "*builtin*" then
		return func
	end

	-- These tail-calls allows passing all return values of `func_o`
	-- also called https://en.wikipedia.org/wiki/Continuation_passing_style
	-- Compared to table creation and unpacking it won't lose `nil` returns
	-- and is expected to be faster
	-- `tracy_ZoneEnd_and_return` / `measure` will be executed after `func_o(...)`

	if do_tracy then
		local func_o = func
		func = function(...)
			tracy.ZoneBeginN(instrument_name_with_source)
			return tracy_ZoneEnd_and_return(func_o(...))
		end
	end

	if do_measure then
		local func_o = func
		func = function(...)
			local start = time()
			return measure(modname, instrument_name, start, func_o(...))
		end
	end

	return func
end

local function can_be_called(func)
	-- It has to be a function or callable table
	return type(func) == "function" or
		((type(func) == "table" or type(func) == "userdata") and
		getmetatable(func) and getmetatable(func).__call)
end

local function assert_can_be_called(func, func_name, level)
	if not can_be_called(func) then
		-- Then throw an *helpful* error, by pointing on our caller instead of us.
		error(format("Invalid argument to %s. Expected function-like type instead of '%s'.",
				func_name, type(func)), level + 1)
	end
end

---
-- Wraps a registration function `func` in such a way,
-- that it will automatically instrument any callback function passed as first argument.
--
local function instrument_register(func, func_name)
	local register_name = func_name:gsub("^register_", "", 1)
	return function(callback, ...)
		assert_can_be_called(callback, func_name, 2)
		return func(instrument {
			func = callback,
			func_name = register_name
		}, ...)
	end
end

local function init_chatcommand()
	if core.settings:get_bool("instrument.chatcommand", true) then
		local orig_register_chatcommand = core.register_chatcommand
		core.register_chatcommand = function(cmd, def)
			def.func = instrument {
				func = def.func,
				label = "/" .. cmd,
			}
			orig_register_chatcommand(cmd, def)
		end
	end
end

---
-- Start instrumenting selected functions
--
local function init()
	if core.settings:get_bool("instrument.entity", true) then
		-- Explicitly declare entity api-methods.
		-- Simple iteration would ignore lookup via __index.
		local entity_instrumentation = {
			"on_activate",
			"on_deactivate",
			"on_step",
			"on_punch",
			"on_death",
			"on_rightclick",
			"on_attach_child",
			"on_detach_child",
			"on_detach",
			"get_staticdata",
		}
		-- Wrap register_entity() to instrument them on registration.
		local orig_register_entity = core.register_entity
		core.register_entity = function(name, def)
			local modname = get_current_modname()
			for _, func_name in ipairs(entity_instrumentation) do
				def[func_name] = instrument {
					func = def[func_name],
					mod = modname,
					func_name = func_name,
					label = name,
				}
			end
			orig_register_entity(name, def)
		end
	end

	if core.settings:get_bool("instrument.abm", true) then
		-- Wrap register_abm() to automatically instrument abms.
		local orig_register_abm = core.register_abm
		core.register_abm = function(spec)
			spec.action = instrument {
				func = spec.action,
				class = "ABM",
				label = spec.label,
			}
			orig_register_abm(spec)
		end
	end

	if core.settings:get_bool("instrument.lbm", true) then
		-- Wrap register_lbm() to automatically instrument lbms.
		local orig_register_lbm = core.register_lbm
		core.register_lbm = function(spec)
			local k = spec.bulk_action ~= nil and "bulk_action" or "action"
			spec[k] = instrument {
				func = spec[k],
				class = "LBM",
				label = spec.label or spec.name,
			}
			orig_register_lbm(spec)
		end
	end

	if core.settings:get_bool("instrument.global_callback", true) then
		for _, func_name in ipairs(register_functions) do
			core[func_name] = instrument_register(core[func_name], func_name)
		end
	end

	if core.settings:get_bool("instrument.profiler", false) then
		-- Measure overhead of instrumentation, but keep it down for functions
		-- So keep the `return` for better optimization.
		profiler.empty_instrument = instrument {
			func = function() return end,
			mod = "*profiler*",
			class = "Instrumentation overhead",
			label = false,
		}
	end
end

return {
	instrument = instrument,
	init = init,
	init_chatcommand = init_chatcommand,
}
