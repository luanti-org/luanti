local modname = core.get_current_modname()
local prefix = "["..modname.."] "

-- Startup info
core.log("action", prefix.."modname="..dump(modname))
core.log("action", prefix.."modpath="..dump(core.get_modpath(modname)))
core.log("action", prefix.."worldpath="..dump(core.get_worldpath()))

-- Callback info
core.register_on_mods_loaded(function()
	core.log("action", prefix.."Callback: on_mods_loaded()")
end)

core.register_on_chatcommand(function(name, command, params)
	core.log("action", prefix.."Caught command '"..command.."', issued by '"..name.."'. Parameters: '"..params.."'")
end)
