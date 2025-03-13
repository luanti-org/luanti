
local scriptpath = core.get_builtin_path()
local commonpath = scriptpath .. "common" .. DIR_DELIM
local gamepath   = scriptpath .. "game".. DIR_DELIM

-- Shared between builtin files, but
-- not exposed to outer context
local builtin_shared = {}

dofile(gamepath .. "constants.lua")
assert(loadfile(commonpath .. "item_s.lua"))(builtin_shared)
assert(loadfile(gamepath .. "item.lua"))(builtin_shared)
assert(loadfile(commonpath .. "register.lua"))(builtin_shared)
assert(loadfile(gamepath .. "register.lua"))(builtin_shared)

if core.settings:get_bool("profiler.load") then
	profiler = dofile(scriptpath .. "profiler" .. DIR_DELIM .. "init.lua")
end

dofile(commonpath .. "after.lua")
dofile(commonpath .. "metatable.lua")
dofile(commonpath .. "mod_storage.lua")
dofile(gamepath .. "item_entity.lua")
dofile(gamepath .. "deprecated.lua")
dofile(gamepath .. "misc_s.lua")
dofile(gamepath .. "misc.lua")
dofile(gamepath .. "privileges.lua")
dofile(gamepath .. "auth.lua")
dofile(commonpath .. "chatcommands.lua")
dofile(gamepath .. "chat.lua")
dofile(commonpath .. "information_formspecs.lua")
dofile(gamepath .. "static_spawn.lua")
dofile(gamepath .. "detached_inventory.lua")
assert(loadfile(gamepath .. "falling.lua"))(builtin_shared)
dofile(gamepath .. "features.lua")
dofile(gamepath .. "voxelarea.lua")
dofile(gamepath .. "forceloading.lua")
dofile(gamepath .. "hud.lua")
dofile(gamepath .. "knockback.lua")
dofile(gamepath .. "async.lua")
dofile(gamepath .. "death_screen.lua")

core.after(0, builtin_shared.cache_content_ids)

profiler = nil
