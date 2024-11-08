local scriptpath = core.get_builtin_path()
local clientpath = scriptpath.."client"..DIR_DELIM
local commonpath = scriptpath.."common"..DIR_DELIM

dofile(clientpath .. "register.lua")
dofile(commonpath .. "after.lua")
dofile(commonpath .. "mod_storage.lua")
dofile(commonpath .. "chatcommands.lua")
dofile(commonpath .. "information_formspecs.lua")
dofile(clientpath .. "chatcommands.lua")
dofile(clientpath .. "misc.lua")
assert(loadfile(commonpath .. "item_s.lua"))({}) -- Just for push/read node functions
