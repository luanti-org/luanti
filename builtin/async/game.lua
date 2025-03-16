core.log("info", "Initializing asynchronous environment (game)")

local function pack2(...)
	return {n=select('#', ...), ...}
end

-- Entrypoint to run async jobs, called by C++
function core.job_processor(func, params)
	local retval = pack2(func(unpack(params, 1, params.n)))

	return retval
end

-- Import a bunch of individual files from builtin/game/
local gamepath = core.get_builtin_path() .. "game" .. DIR_DELIM
local commonpath = core.get_builtin_path() .. "common" .. DIR_DELIM

local builtin_shared = {}

dofile(gamepath .. "constants.lua")
assert(loadfile(commonpath .. "item_s.lua"))(builtin_shared)
dofile(gamepath .. "misc_s.lua")
dofile(gamepath .. "features.lua")
dofile(gamepath .. "voxelarea.lua")

-- Transfer of globals
do
	local all = assert(core.transferred_globals)
	core.transferred_globals = nil

	all.registered_nodes = {}
	all.registered_craftitems = {}
	all.registered_tools = {}
	for k, v in pairs(all.registered_items) do
		-- Ignore new keys
		setmetatable(v, {__newindex = function() end})
		-- Reassemble the other tables
		if v.type == "node" then
			getmetatable(v).__index = all.nodedef_default
			all.registered_nodes[k] = v
		elseif v.type == "craft" then
			getmetatable(v).__index = all.craftitemdef_default
			all.registered_craftitems[k] = v
		elseif v.type == "tool" then
			getmetatable(v).__index = all.tooldef_default
			all.registered_tools[k] = v
		else
			getmetatable(v).__index = all.noneitemdef_default
		end
	end

	for k, v in pairs(all) do
		core[k] = v
	end
end

-- For tables that are indexed by item name:
-- If table[X] does not exist, default to table[core.registered_aliases[X]]
local alias_metatable = {
	__index = function(t, name)
		return rawget(t, core.registered_aliases[name])
	end,
	__newindex = function()
		error("table is read-only")
	end
}
setmetatable(core.registered_items, alias_metatable)
setmetatable(core.registered_nodes, alias_metatable)
setmetatable(core.registered_craftitems, alias_metatable)
setmetatable(core.registered_tools, alias_metatable)

builtin_shared.cache_content_ids()
