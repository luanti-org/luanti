-- Luanti
-- Copyright (C) 2016 T4im
-- SPDX-License-Identifier: LGPL-2.1-or-later

local S = core.get_translator("__builtin")

local profiler_path = core.get_builtin_path().."profiler"..DIR_DELIM
local profiler = {}
local sampler = assert(loadfile(profiler_path .. "sampling.lua"))(profiler)
local instrumentation  = assert(loadfile(profiler_path .. "instrumentation.lua"))(profiler, sampler)
local reporter = dofile(profiler_path .. "reporter.lua")
profiler.instrument = instrumentation.instrument

---
-- Delayed registration of the /profiler chat command
-- Is called later, after `core.register_chatcommand` was set up.
--
function profiler.init_chatcommand()
	local instrument_profiler = core.settings:get_bool("instrument.profiler", false)
	if instrument_profiler then
		instrumentation.init_chatcommand()
	end

	local param_usage = S("print [<filter>] | dump [<filter>] | save [<format> [<filter>]] | reset")
	core.register_chatcommand("profiler", {
		description = S("Handle the profiler and profiling data. "
			.. "Can output to chat (print), action log (dump), or file in world (save). "
			.. "Format can be one of txt, csv, lua, json, json_pretty (structures may be subject to change). "
			.. "Filter is a lua pattern used to limit output to matching mod names."),
		params = param_usage,
		privs = { server=true },
		func = function(name, param)
			local command, arg0 = string.match(param, "([^ ]+) ?(.*)")
			local args = arg0 and string.split(arg0, " ")

			if command == "dump" then
				core.log("action", reporter.print(sampler.profile, arg0, false))
				return true, S("Statistics written to action log.")
			elseif command == "print" then
				return true, reporter.print(sampler.profile, arg0, true)
			elseif command == "save" then
				return reporter.save(sampler.profile, args[1] or "txt", args[2])
			elseif command == "reset" then
				sampler.reset()
				return true, S("Statistics were reset.")
			end

			return false
		end
	})

	if not instrument_profiler then
		instrumentation.init_chatcommand()
	end
end

sampler.init()
instrumentation.init()

return profiler
