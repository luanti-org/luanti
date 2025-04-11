local scriptpath = core.get_builtin_path()
local pausepath = scriptpath.."pause_menu"..DIR_DELIM
local commonpath = scriptpath.."common"..DIR_DELIM

-- we're in-game, so no absolute paths are needed
defaulttexturedir = ""

local builtin_shared = {}

assert(loadfile(commonpath .. "register.lua"))(builtin_shared)
assert(loadfile(commonpath .. "menu.lua"))(builtin_shared)
assert(loadfile(pausepath .. "register.lua"))(builtin_shared)
dofile(commonpath .. "settings" .. DIR_DELIM .. "init.lua")
