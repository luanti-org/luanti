local builtin_shared = ...

-- Copy all the registration tables over
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

--
-- Callbacks
--

local make_registration = builtin_shared.make_registration

core.registered_on_mods_loaded, core.register_on_mods_loaded = make_registration()
core.registered_on_generateds, core.register_on_generated = make_registration()
core.registered_on_shutdown, core.register_on_shutdown = make_registration()
