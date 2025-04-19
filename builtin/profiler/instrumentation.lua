-- Luanti
-- Copyright (C) 2016 T4im
-- SPDX-License-Identifier: LGPL-2.1-or-later

local format, pairs, type = string.format, pairs, type
local core, get_current_modname = core, core.get_current_modname
local profiler, sampler, get_bool_default = ...

local instrument_builtin = get_bool_default("instrument.builtin", false)

local register_functions = {
	register_globalstep = 0,
	register_playerevent = 0,
	register_on_placenode = 0,
	register_on_dignode = 0,
	register_on_punchnode = 0,
	register_on_generated = 0,
	register_on_newplayer = 0,
	register_on_dieplayer = 0,
	register_on_respawnplayer = 0,
	register_on_prejoinplayer = 0,
	register_on_joinplayer = 0,
	register_on_leaveplayer = 0,
	register_on_cheat = 0,
	register_on_chat_message = 0,
	register_on_player_receive_fields = 0,
	register_on_craft = 0,
	register_craft_predict = 0,
	register_on_protection_violation = 0,
	register_on_item_eat = 0,
	register_on_punchplayer = 0,
	register_on_player_hpchange = 0,
	register_on_mapblocks_changed = 0,
}

local function regex_escape(s)
	return s:gsub("(%W)", "%%%1")
end

---
-- Create an unique instrument name.
-- Generate a missing label with a running index number.
--
local counts = {}
local worldmods_path = regex_escape(core.get_worldpath())
local user_path = regex_escape(core.get_user_path())
local builtin_path = regex_escape(core.get_builtin_path())
local function generate_name(def)
	local class, label, func_name = def.class, def.label, def.func_name
	if label then
		if class or func_name then
			return format("%s '%s' %s", class or "", label, func_name or ""):trim()
		end
		return format("%s", label):trim()
	elseif label == false then
		return format("%s", class or func_name):trim()
	end

	local index_id = def.mod .. (class or func_name)
	local index = counts[index_id] or 1
	counts[index_id] = index + 1
	local info = debug.getinfo(def.func)
	local modpath = regex_escape(core.get_modpath(def.mod) or "")
	local source = info.source
	if modpath ~= "" then
		source = source:gsub(modpath, def.mod)
	end
	source = source:gsub(worldmods_path, "")
	source = source:gsub(builtin_path, "builtin" .. DIR_DELIM)
	source = source:gsub(user_path, "")
	return format("%s[%d] %s#%s", class or func_name, index, source, info.linedefined)
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
	local instrument_name = generate_name(def)
	local func = def.func

	if not instrument_builtin and modname == "*builtin*" then
		return func
	end

	return function(...)
		-- This tail-call allows passing all return values of `func`
		-- also called https://en.wikipedia.org/wiki/Continuation_passing_style
		-- Compared to table creation and unpacking it won't lose `nil` returns
		-- and is expected to be faster
		-- `measure` will be executed after func(...)
		local start = time()
		return measure(modname, instrument_name, start, func(...))
	end
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
		register_functions[func_name] = register_functions[func_name] + 1
		return func(instrument {
			func = callback,
			func_name = register_name
		}, ...)
	end
end

local function init_chatcommand()
	if get_bool_default("instrument.chatcommand", true) then
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
	if get_bool_default("instrument.entity", true) then
		-- Explicitly declare entity api-methods.
		-- Simple iteration would ignore lookup via __index.
		local entity_instrumentation = {
			"on_activate",
			"on_deactivate",
			"on_step",
			"on_punch",
			"on_rightclick",
			"get_staticdata",
		}
		-- Wrap register_entity() to instrument them on registration.
		local orig_register_entity = core.register_entity
		core.register_entity = function(name, prototype)
			local modname = get_current_modname()
			for _, func_name in pairs(entity_instrumentation) do
				prototype[func_name] = instrument {
					func = prototype[func_name],
					mod = modname,
					func_name = func_name,
					label = prototype.label,
				}
			end
			orig_register_entity(name,prototype)
		end
	end

	if get_bool_default("instrument.abm", true) then
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

	if get_bool_default("instrument.lbm", true) then
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

	if get_bool_default("instrument.global_callback", true) then
		for func_name, _ in pairs(register_functions) do
			core[func_name] = instrument_register(core[func_name], func_name)
		end
	end

	if get_bool_default("instrument.profiler", false) then
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
	register_functions = register_functions,
	instrument = instrument,
	init = init,
	init_chatcommand = init_chatcommand,
}
