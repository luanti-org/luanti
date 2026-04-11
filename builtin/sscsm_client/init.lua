local scriptpath = core.get_builtin_path()
local commonpath = scriptpath .. "common" .. DIR_DELIM
local mypath     = scriptpath .. "sscsm_client".. DIR_DELIM

-- Shared between builtin files, but
-- not exposed to outer context
local builtin_shared = {}

-- Populated by SSCSMEventUpdateContentDefs before mods load.
core.registered_content_ids = {}
core.registered_content_names = {}

function core.get_content_id(name)
	return core.registered_content_ids[name]
end
function core.get_name_from_content_id(id)
	return core.registered_content_names[id]
end

dofile(commonpath .. "vector.lua")
assert(loadfile(commonpath .. "item_s.lua"))(builtin_shared)
assert(loadfile(commonpath .. "register.lua"))(builtin_shared)
assert(loadfile(mypath .. "register.lua"))(builtin_shared)

dofile(commonpath .. "after.lua")

-- unset, as promised in initializeSecuritySSCSM()
debug.getinfo = nil
