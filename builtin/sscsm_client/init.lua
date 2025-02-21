
local scriptpath = core.get_builtin_path()
local commonpath = scriptpath .. "common" .. DIR_DELIM
local mypath     = scriptpath .. "sscsm_client".. DIR_DELIM

-- Shared between builtin files, but
-- not exposed to outer context
local builtin_shared = {}

assert(loadfile(commonpath .. "register.lua"))(builtin_shared)
assert(loadfile(mypath .. "register.lua"))(builtin_shared)

dofile(commonpath .. "after.lua")
