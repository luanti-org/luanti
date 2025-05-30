-- Luanti
-- Copyright (C) 2016 T4im
-- SPDX-License-Identifier: LGPL-2.1-or-later

local S = core.get_translator("__builtin")

local function get_bool_default(name, default)
	local val = core.settings:get_bool(name)
	if val == nil then
		return default
	end
	return val
end

local profiler_path = core.get_builtin_path().."profiler"..DIR_DELIM
local profiler = {}
local sampler = assert(loadfile(profiler_path .. "sampling.lua"))(profiler)
local instrumentation  = assert(loadfile(profiler_path .. "instrumentation.lua"))(profiler, sampler, get_bool_default)
local reporter = dofile(profiler_path .. "reporter.lua")
profiler.instrument = instrumentation.instrument

---
-- Delayed registration of the /profiler chat command
-- Is called later, after `core.register_chatcommand` was set up.
--
function profiler.init_chatcommand()
	local instrument_profiler = get_bool_default("instrument.profiler", false)
	if instrument_profiler then
		instrumentation.init_chatcommand()
	end

	local param_usage = S("print [<filter>] | dump [<filter>] | save [<format> [<filter>]] | reset")
	core.register_chatcommand("profiler", {
		description = S("Handle the profiler and profiling data"),
		params = param_usage,
		privs = { server=true },
		func = function(name, param)
			local command, arg0 = string.match(param, "([^ ]+) ?(.*)")
			local args = arg0 and string.split(arg0, " ")

			if command == "dump" then
				core.log("action", reporter.print(sampler.profile, arg0))
				return true, S("Statistics written to action log.")
			elseif command == "print" then
				return true, reporter.print(sampler.profile, arg0)
			elseif command == "save" then
				return reporter.save(sampler.profile, args[1] or "txt", args[2])
			elseif command == "reset" then
				sampler.reset()
				return true, S("Statistics were reset.")
			end

			return false,
				S("Usage: @1", param_usage) .. "\n" ..
				S("Format can be one of txt, csv, lua, json, json_pretty (structures may be subject to change).")
		end
	})

	if not instrument_profiler then
		instrumentation.init_chatcommand()
	end
end

sampler.init()
instrumentation.init()

return profiler
