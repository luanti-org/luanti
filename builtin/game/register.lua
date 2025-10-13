local builtin_shared = ...
local S = core.get_translator("__builtin")
local debug_getinfo = debug.getinfo

--
-- Make raw registration functions inaccessible to anyone except this file
--

local register_item_raw = core.register_item_raw
core.register_item_raw = nil

local unregister_item_raw = core.unregister_item_raw
core.unregister_item_raw = nil

local register_alias_raw = core.register_alias_raw
core.register_alias_raw = nil

--
-- Item / entity / ABM / LBM registration functions
--

core.registered_abms = {}
core.registered_lbms = {}
core.registered_entities = {}
core.registered_items = {}
core.registered_nodes = {}
core.registered_craftitems = {}
core.registered_tools = {}
core.registered_aliases = {}

-- For tables that are indexed by item name:
-- If table[X] does not exist, default to table[core.registered_aliases[X]]
local alias_metatable = {
	__index = function(t, name)
		return rawget(t, core.registered_aliases[name])
	end
}
setmetatable(core.registered_items, alias_metatable)
setmetatable(core.registered_nodes, alias_metatable)
setmetatable(core.registered_craftitems, alias_metatable)
setmetatable(core.registered_tools, alias_metatable)

-- These item names may not be used because they would interfere
-- with legacy itemstrings
local forbidden_item_names = {
	MaterialItem = true,
	MaterialItem2 = true,
	MaterialItem3 = true,
	NodeItem = true,
	node = true,
	CraftItem = true,
	craft = true,
	MBOItem = true,
	ToolItem = true,
	tool = true,
}

local function check_modname_prefix(name)
	if name:sub(1,1) == ":" then
		-- If the name starts with a colon, we can skip the modname prefix
		-- mechanism.
		return name:sub(2)
	else
		-- Enforce that the name starts with the correct mod name.
		local expected_prefix = (core.get_current_modname() or "") .. ":"
		if name:sub(1, #expected_prefix) ~= expected_prefix then
			error("Name " .. name .. " does not follow naming conventions: " ..
				"\"" .. expected_prefix .. "\" or \":\" prefix required")
		end

		-- Enforce that the name only contains letters, numbers and underscores.
		local subname = name:sub(#expected_prefix+1)
		if subname:find("[^%w_]") then
			error("Name " .. name .. " does not follow naming conventions: " ..
				"contains unallowed characters")
		end

		return name
	end
end

local function check_node_list(list, field)
	local t = type(list)
	if t == "table" then
		for _, entry in pairs(list) do
			assert(type(entry) == "string",
				"Field '" .. field .. "' contains non-string entry")
		end
	elseif t ~= "string" and t ~= "nil" then
		error("Field '" .. field .. "' has invalid type " .. t)
	end
end

function core.register_abm(spec)
	-- Add to core.registered_abms
	check_node_list(spec.nodenames, "nodenames")
	check_node_list(spec.neighbors, "neighbors")
	assert(type(spec.action) == "function", "Required field 'action' of type function")

	core.registered_abms[#core.registered_abms + 1] = spec
	spec.mod_origin = core.get_current_modname() or "??"
end

function core.register_lbm(spec)
	-- Add to core.registered_lbms
	check_modname_prefix(spec.name)
	check_node_list(spec.nodenames, "nodenames")
	local have = spec.action ~= nil
	local have_bulk = spec.bulk_action ~= nil
	assert(not have or type(spec.action) == "function", "Field 'action' must be a function")
	assert(not have_bulk or type(spec.bulk_action) == "function", "Field 'bulk_action' must be a function")
	assert(have ~= have_bulk, "Either 'action' or 'bulk_action' must be present")

	core.registered_lbms[#core.registered_lbms + 1] = spec
	spec.mod_origin = core.get_current_modname() or "??"
end

function core.register_entity(name, prototype)
	-- Check name
	if name == nil then
		error("Unable to register entity: Name is nil")
	end
	name = check_modname_prefix(tostring(name))

	prototype.name = name
	prototype.__index = prototype  -- so that it can be used as a metatable

	-- Add to core.registered_entities
	core.registered_entities[name] = prototype
	prototype.mod_origin = core.get_current_modname() or "??"
end

local function preprocess_node(nodedef)
	-- Use the nodebox as selection box if it's not set manually
	if nodedef.drawtype == "nodebox" and not nodedef.selection_box then
		nodedef.selection_box = nodedef.node_box
	elseif nodedef.drawtype == "fencelike" and not nodedef.selection_box then
		nodedef.selection_box = {
			type = "fixed",
			fixed = {-1/8, -1/2, -1/8, 1/8, 1/2, 1/8},
		}
	end

	if nodedef.light_source and nodedef.light_source > core.LIGHT_MAX then
		nodedef.light_source = core.LIGHT_MAX
		core.log("warning", "Node 'light_source' value exceeds maximum," ..
			" limiting it: " .. nodedef.name)
	end

	-- Flowing liquid uses param2
	if nodedef.liquidtype == "flowing" then
		nodedef.paramtype2 = "flowingliquid"
	end
end

local function preprocess_craft(itemdef)
	-- BEGIN Legacy stuff
	if itemdef.inventory_image == nil and itemdef.image ~= nil then
		core.log("deprecated", "The `image` field in craftitem definitions " ..
			"is deprecated. Use `inventory_image` instead. " ..
			"Craftitem name: " .. itemdef.name, 3)
		itemdef.inventory_image = itemdef.image
	end
	-- END Legacy stuff
end

local function preprocess_tool(tooldef)
	tooldef.stack_max = 1

	-- BEGIN Legacy stuff
	if tooldef.inventory_image == nil and tooldef.image ~= nil then
		core.log("deprecated", "The `image` field in tool definitions " ..
			"is deprecated. Use `inventory_image` instead. " ..
			"Tool name: " .. tooldef.name, 3)
		tooldef.inventory_image = tooldef.image
	end

	if tooldef.tool_capabilities == nil and
	   (tooldef.full_punch_interval ~= nil or
	    tooldef.basetime ~= nil or
	    tooldef.dt_weight ~= nil or
	    tooldef.dt_crackiness ~= nil or
	    tooldef.dt_crumbliness ~= nil or
	    tooldef.dt_cuttability ~= nil or
	    tooldef.basedurability ~= nil or
	    tooldef.dd_weight ~= nil or
	    tooldef.dd_crackiness ~= nil or
	    tooldef.dd_crumbliness ~= nil or
	    tooldef.dd_cuttability ~= nil) then
		core.log("deprecated", "Specifying tool capabilities directly in the tool " ..
			"definition is deprecated. Use the `tool_capabilities` field instead. " ..
			"Tool name: " .. tooldef.name, 3)
		tooldef.tool_capabilities = {
			full_punch_interval = tooldef.full_punch_interval,
			basetime = tooldef.basetime,
			dt_weight = tooldef.dt_weight,
			dt_crackiness = tooldef.dt_crackiness,
			dt_crumbliness = tooldef.dt_crumbliness,
			dt_cuttability = tooldef.dt_cuttability,
			basedurability = tooldef.basedurability,
			dd_weight = tooldef.dd_weight,
			dd_crackiness = tooldef.dd_crackiness,
			dd_crumbliness = tooldef.dd_crumbliness,
			dd_cuttability = tooldef.dd_cuttability,
		}
	end
	-- END Legacy stuff

	-- Automatically set punch_attack_uses as a convenience feature
	local toolcaps = tooldef.tool_capabilities
	if toolcaps and toolcaps.punch_attack_uses == nil then
		for _, cap in pairs(toolcaps.groupcaps or {}) do
			local level = (cap.maxlevel or 0) - 1
			if (cap.uses or 0) ~= 0 and level >= 0 then
				toolcaps.punch_attack_uses = cap.uses * (3 ^ level)
				break
			end
		end
	end
end

local default_tables = {
	node = core.nodedef_default,
	craft = core.craftitemdef_default,
	tool = core.tooldef_default,
	none = core.noneitemdef_default,
}

local preprocess_fns = {
	node = preprocess_node,
	craft = preprocess_craft,
	tool = preprocess_tool,
}

function core.register_item(name, itemdef)
	-- Check name
	if name == nil then
		error("Unable to register item: Name is nil")
	end
	name = check_modname_prefix(tostring(name))
	if forbidden_item_names[name] then
		error("Unable to register item: Name is forbidden: " .. name)
	end

	itemdef.name = name

	-- Compatibility stuff depending on type
	local fn = preprocess_fns[itemdef.type]
	if fn then
		fn(itemdef)
	end

	-- Apply defaults
	local defaults = default_tables[itemdef.type]
	if defaults == nil then
		error("Unable to register item: Type is invalid: " .. dump(itemdef))
	end
	local old_mt = getmetatable(itemdef)
	-- TODO most of these checks should become an error after a while (maybe in 2026?)
	if old_mt ~= nil and next(old_mt) ~= nil then
		-- Note that even registering multiple identical items with the same table
		-- is not allowed, due to the 'name' property.
		if old_mt.__index == defaults then
			core.log("warning", "Item definition table was reused between registrations. "..
				"This is unsupported and broken: " .. name)
		else
			core.log("warning", "Item definition has a metatable, this is "..
				"unsupported and it will be overwritten: " .. name)
		end
	end
	setmetatable(itemdef, {__index = defaults})

	-- BEGIN Legacy stuff
	if itemdef.cookresult_itemstring ~= nil and itemdef.cookresult_itemstring ~= "" then
		core.log("deprecated", "The `cookresult_itemstring` item definition " ..
			"field is deprecated. Use `core.register_craft` instead. " ..
			"Item name: " .. itemdef.name, 2)
		core.register_craft({
			type="cooking",
			output=itemdef.cookresult_itemstring,
			recipe=itemdef.name,
			cooktime=itemdef.furnace_cooktime
		})
	end
	if itemdef.furnace_burntime ~= nil and itemdef.furnace_burntime >= 0 then
		core.log("deprecated", "The `furnace_burntime` item definition " ..
			"field is deprecated. Use `core.register_craft` instead. " ..
			"Item name: " .. itemdef.name, 2)
		core.register_craft({
			type="fuel",
			recipe=itemdef.name,
			burntime=itemdef.furnace_burntime
		})
	end
	-- END Legacy stuff

	itemdef.mod_origin = core.get_current_modname() or "??"

	-- Ignore new keys as a failsafe to prevent mistakes
	getmetatable(itemdef).__newindex = function() end

	-- Add to registered_* tables
	if itemdef.type == "node" then
		core.registered_nodes[itemdef.name] = itemdef
	elseif itemdef.type == "craft" then
		core.registered_craftitems[itemdef.name] = itemdef
	elseif itemdef.type == "tool" then
		core.registered_tools[itemdef.name] = itemdef
	end
	core.registered_items[itemdef.name] = itemdef
	core.registered_aliases[itemdef.name] = nil

	register_item_raw(itemdef)
end

local function make_register_item_wrapper(the_type)
	return function(name, itemdef)
		itemdef.type = the_type
		return core.register_item(name, itemdef)
	end
end

core.register_node = make_register_item_wrapper("node")
core.register_craftitem = make_register_item_wrapper("craft")
core.register_tool = make_register_item_wrapper("tool")

function core.unregister_item(name)
	if not core.registered_items[name] then
		core.log("warning", "Not unregistering item " ..name..
			" because it doesn't exist.")
		return
	end
	-- Erase from registered_* table
	core.registered_nodes[name] = nil
	core.registered_craftitems[name] = nil
	core.registered_tools[name] = nil
	core.registered_items[name] = nil

	unregister_item_raw(name)
end

function core.register_alias(name, convert_to)
	if forbidden_item_names[name] then
		error("Unable to register alias: Name is forbidden: " .. name)
	end
	if core.registered_items[name] ~= nil then
		core.log("warning", "Not registering alias, item with same name" ..
			" is already defined: " .. name .. " -> " .. convert_to)
	else
		core.registered_aliases[name] = convert_to
		register_alias_raw(name, convert_to)
	end
end

function core.register_alias_force(name, convert_to)
	if forbidden_item_names[name] then
		error("Unable to register alias: Name is forbidden: " .. name)
	end
	if core.registered_items[name] ~= nil then
		core.unregister_item(name)
		core.log("info", "Removed item " ..name..
			" while attempting to force add an alias")
	end
	core.registered_aliases[name] = convert_to
	register_alias_raw(name, convert_to)
end

function core.on_craft(itemstack, player, old_craft_list, craft_inv)
	for _, func in ipairs(core.registered_on_crafts) do
		-- cast to ItemStack since func() could return a string
		itemstack = ItemStack(func(itemstack, player, old_craft_list, craft_inv) or itemstack)
	end
	return itemstack
end

function core.craft_predict(itemstack, player, old_craft_list, craft_inv)
	for _, func in ipairs(core.registered_craft_predicts) do
		-- cast to ItemStack since func() could return a string
		itemstack = ItemStack(func(itemstack, player, old_craft_list, craft_inv) or itemstack)
	end
	return itemstack
end

-- Alias the forbidden item names to "" so they can't be
-- created via itemstrings (e.g. /give)
for name in pairs(forbidden_item_names) do
	core.registered_aliases[name] = ""
	register_alias_raw(name, "")
end

--
-- Built-in node definitions. Also defined in C.
--

core.register_item(":unknown", {
	type = "none",
	description = S("Unknown Item"),
	inventory_image = "unknown_item.png",
	on_place = core.item_place,
	on_secondary_use = core.item_secondary_use,
	on_drop = core.item_drop,
	groups = {not_in_creative_inventory=1},
	diggable = true,
})

core.register_node(":air", {
	description = S("Air"),
	inventory_image = "air.png",
	wield_image = "air.png",
	drawtype = "airlike",
	paramtype = "light",
	sunlight_propagates = true,
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	floodable = true,
	air_equivalent = true,
	drop = "",
	groups = {not_in_creative_inventory=1},
})

core.register_node(":ignore", {
	description = S("Ignore"),
	inventory_image = "ignore.png",
	wield_image = "ignore.png",
	drawtype = "airlike",
	paramtype = "none",
	sunlight_propagates = false,
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true, -- A way to remove accidentally placed ignores
	air_equivalent = true,
	drop = "",
	groups = {not_in_creative_inventory=1},
	node_placement_prediction = "",
	on_place = function(itemstack, placer, pointed_thing)
		core.chat_send_player(
				placer:get_player_name(),
				core.colorize("#FF0000",
				S("You can't place 'ignore' nodes!")))
		return ""
	end,
})

-- The hand (bare definition)
core.register_item(":", {
	type = "none",
	wield_image = "wieldhand.png",
	groups = {not_in_creative_inventory=1},
})

local itemdefs_finalized = false

function core.override_item(name, redefinition, del_fields)
	if redefinition.name ~= nil then
		error("Attempt to redefine name of "..name.." to "..dump(redefinition.name), 2)
	end
	if redefinition.type ~= nil then
		error("Attempt to redefine type of "..name.." to "..dump(redefinition.type), 2)
	end
	local item = core.registered_items[name]
	if not item then
		error("Attempt to override non-existent item "..name, 2)
	end
	if itemdefs_finalized then
		-- TODO: it's not clear if this needs to be allowed at all?
		core.log("warning", "Overriding item " .. name .. " after server startup. " ..
			"This is unsupported and can cause problems related to data inconsistency.")
	end
	for k, v in pairs(redefinition) do
		rawset(item, k, v)
	end
	for _, field in ipairs(del_fields or {}) do
		assert(field ~= "name" and field ~= "type")
		rawset(item, field, nil)
	end
	register_item_raw(item)
end

function core.run_priv_callbacks(name, priv, caller, method)
	local def = core.registered_privileges[priv]
	if not def or not def["on_" .. method] or
			not def["on_" .. method](name, caller) then
		for _, func in ipairs(core["registered_on_priv_" .. method]) do
			if not func(name, caller, priv) then
				break
			end
		end
	end
end

--
-- Callback registration
--

local function make_registration_wrap(reg_fn_name, clear_fn_name)
	local list = {}

	local orig_reg_fn = core[reg_fn_name]
	core[reg_fn_name] = function(def)
		local retval = orig_reg_fn(def)
		if retval ~= nil then
			if def.name ~= nil then
				list[def.name] = def
			else
				list[retval] = def
			end
		end
		return retval
	end

	local orig_clear_fn = core[clear_fn_name]
	core[clear_fn_name] = function()
		for k in pairs(list) do
			list[k] = nil
		end
		return orig_clear_fn()
	end

	return list
end

local function make_wrap_deregistration(reg_fn, clear_fn, list)
	local unregister = function (key)
		if type(key) ~= "string" then
			error("key is not a string", 2)
		end
		if not list[key] then
			error("Attempt to unregister non-existent element - '" .. key .. "'", 2)
		end
		local temporary_list = table.copy(list)
		clear_fn()
		for k,v in pairs(temporary_list) do
			if key ~= k then
				reg_fn(v)
			end
		end
	end
	return unregister
end

core.registered_on_player_hpchanges = { modifiers = { }, loggers = { } }

function core.registered_on_player_hpchange(player, hp_change, reason)
	local last
	for i = #core.registered_on_player_hpchanges.modifiers, 1, -1 do
		local func = core.registered_on_player_hpchanges.modifiers[i]
		hp_change, last = func(player, hp_change, reason)
		if type(hp_change) ~= "number" then
			local debuginfo = debug_getinfo(func)
			error("The register_on_hp_changes function has to return a number at " ..
				debuginfo.short_src .. " line " .. debuginfo.linedefined)
		end
		if last then
			break
		end
	end
	for i, func in ipairs(core.registered_on_player_hpchanges.loggers) do
		func(player, hp_change, reason)
	end
	return hp_change
end

function core.register_on_player_hpchange(func, modifier)
	if modifier then
		core.registered_on_player_hpchanges.modifiers[#core.registered_on_player_hpchanges.modifiers + 1] = func
	else
		core.registered_on_player_hpchanges.loggers[#core.registered_on_player_hpchanges.loggers + 1] = func
	end
	core.callback_origins[func] = {
		mod = core.get_current_modname() or "??",
		name = debug_getinfo(1, "n").name or "??"
	}
end

core.registered_biomes      = make_registration_wrap("register_biome",      "clear_registered_biomes")
core.registered_ores        = make_registration_wrap("register_ore",        "clear_registered_ores")
core.registered_decorations = make_registration_wrap("register_decoration", "clear_registered_decorations")

core.unregister_biome = make_wrap_deregistration(core.register_biome,
		core.clear_registered_biomes, core.registered_biomes)

local make_registration = builtin_shared.make_registration
local make_registration_reverse = builtin_shared.make_registration_reverse

-- keep in sync with profiler/instrumentation.lua
core.registered_on_chat_messages, core.register_on_chat_message = make_registration()
core.registered_on_chatcommands, core.register_on_chatcommand = make_registration()
core.registered_globalsteps, core.register_globalstep = make_registration()
core.registered_playerevents, core.register_playerevent = make_registration()
core.registered_on_mods_loaded, core.register_on_mods_loaded = make_registration()
core.registered_on_shutdown, core.register_on_shutdown = make_registration()
core.registered_on_punchnodes, core.register_on_punchnode = make_registration()
core.registered_on_placenodes, core.register_on_placenode = make_registration()
core.registered_on_dignodes, core.register_on_dignode = make_registration()
core.registered_on_generateds, core.register_on_generated = make_registration()
core.registered_on_newplayers, core.register_on_newplayer = make_registration()
core.registered_on_dieplayers, core.register_on_dieplayer = make_registration()
core.registered_on_respawnplayers, core.register_on_respawnplayer = make_registration()
core.registered_on_prejoinplayers, core.register_on_prejoinplayer = make_registration()
core.registered_on_joinplayers, core.register_on_joinplayer = make_registration()
core.registered_on_leaveplayers, core.register_on_leaveplayer = make_registration()
core.registered_on_player_receive_fields, core.register_on_player_receive_fields = make_registration_reverse()
core.registered_on_cheats, core.register_on_cheat = make_registration()
core.registered_on_crafts, core.register_on_craft = make_registration()
core.registered_craft_predicts, core.register_craft_predict = make_registration()
core.registered_on_protection_violation, core.register_on_protection_violation = make_registration()
core.registered_on_item_eats, core.register_on_item_eat = make_registration()
core.registered_on_item_pickups, core.register_on_item_pickup = make_registration()
core.registered_on_punchplayers, core.register_on_punchplayer = make_registration()
core.registered_on_priv_grant, core.register_on_priv_grant = make_registration()
core.registered_on_priv_revoke, core.register_on_priv_revoke = make_registration()
core.registered_on_authplayers, core.register_on_authplayer = make_registration()
core.registered_can_bypass_userlimit, core.register_can_bypass_userlimit = make_registration()
core.registered_on_modchannel_message, core.register_on_modchannel_message = make_registration()
core.registered_on_player_inventory_actions, core.register_on_player_inventory_action = make_registration()
core.registered_allow_player_inventory_actions, core.register_allow_player_inventory_action = make_registration()
core.registered_on_rightclickplayers, core.register_on_rightclickplayer = make_registration()
core.registered_on_liquid_transformed, core.register_on_liquid_transformed = make_registration()
core.registered_on_mapblocks_changed, core.register_on_mapblocks_changed = make_registration()

-- A bunch of registrations are read by the C++ side once on env init, so we cannot
-- allow them to change afterwards (see s_env.cpp).
-- Nodes and items do not have this problem but there are obvious consistency
-- problems if this would be allowed.

local function freeze_table(t)
	-- Freezing a Lua table is not actually possible without some very intrusive
	-- metatable hackery, but we can trivially prevent new additions.
	local mt = table.copy(getmetatable(t) or {})
	mt.__newindex = function()
		error("modification forbidden")
	end
	setmetatable(t, mt)
end

local function generic_reg_error(what)
	return function(something)
		local described = what
		if type(something) == "table" and type(something.name) == "string" then
			described = what .. " " .. something.name
		elseif type(something) == "string" then
			described = what .. " " .. something
		end
		error("Tried to register " .. described .. " after load time!")
	end
end

core.register_on_mods_loaded(function()
	core.after(0, function()
		itemdefs_finalized = true

		-- prevent direct modification
		freeze_table(core.registered_abms)
		freeze_table(core.registered_lbms)
		freeze_table(core.registered_items)
		freeze_table(core.registered_nodes)
		freeze_table(core.registered_craftitems)
		freeze_table(core.registered_tools)
		freeze_table(core.registered_aliases)
		freeze_table(core.registered_on_mapblocks_changed)

		-- neutralize registration functions
		core.register_abm = generic_reg_error("ABM")
		core.register_lbm = generic_reg_error("LBM")
		core.register_item = generic_reg_error("item")
		core.unregister_item = function(name)
			error("Refusing to unregister item " .. name .. " after load time")
		end
		core.register_alias = generic_reg_error("alias")
		core.register_alias_force = generic_reg_error("alias")
		core.register_on_mapblocks_changed = generic_reg_error("on_mapblocks_changed callback")
	end)
end)

--
-- Compatibility for on_mapgen_init()
--

core.register_on_mapgen_init = function(func) func(core.get_mapgen_params()) end
