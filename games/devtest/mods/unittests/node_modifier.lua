-- Applied modifiers are separate from ordinary metadata fields.
core.register_node_modifier("unittests:modifier_first", {light_source = 1})
core.register_node_modifier("unittests:modifier_second", {light_source = 2})

local first = "unittests:modifier_first"
local second = "unittests:modifier_second"
local unregistered = "unittests:unregistered_modifier"
local incorrect = "incorrect"

local function test_modifier_methods(meta)
	
	assert(meta:get_modifiers():add(unregistered))
	assert(meta:get_modifiers():get_list()[1] == unregistered)
	assert(meta:get_modifiers():add(unregistered))
	assert(#meta:get_modifiers():get_list() == 1)
	meta:get_modifiers():remove(unregistered)
	assert(meta:get_modifiers():add(first))
	assert(meta:get_modifiers():add(unregistered, true))
	local unknown_names = meta:get_modifiers():get_list()
	assert(#unknown_names == 2 and unknown_names[1] == first
			and unknown_names[2] == unregistered)
	assert(meta:get_modifiers():add(second, true))
	unknown_names = meta:get_modifiers():get_list()
	assert(#unknown_names == 2 and unknown_names[1] == unregistered
			and unknown_names[2] == second)
	assert(meta:get_modifiers():set_list({}))
	assert(not pcall(meta:get_modifiers().add, meta:get_modifiers(), ""))
	assert(not pcall(meta:get_modifiers().add, meta:get_modifiers(), incorrect))
	assert(#meta:get_modifiers():get_list() == 0)
	assert(not pcall(meta:get_modifiers().remove, meta:get_modifiers(), incorrect))
	meta:get_modifiers():remove(unregistered)
	assert(meta:get_modifiers():add(first))
	assert(meta:get_modifiers():add(second))
	-- Since the modifier is already applied, it should be moved to the end
	assert(meta:get_modifiers():add(first))
	local names = meta:get_modifiers():get_list()
	assert(#names == 2 and names[1] == second and names[2] == first)
	-- Check that the returned table is a copy, and modifying it won't change actual modifiers
	names[1] = first
	assert(meta:get_modifiers():get_list()[1] == second)
	meta:get_modifiers():remove(second)
	names = meta:get_modifiers():get_list()
	assert(#names == 1 and names[1] == first)
	meta:get_modifiers():remove(second)
	names = meta:get_modifiers():get_list()
	assert(#names == 1 and names[1] == first)
	meta:get_modifiers():remove(first)
	assert(#meta:get_modifiers():get_list() == 0)

	for _, invalid in ipairs({"", incorrect, 42, false, {}}) do
		assert(not pcall(meta:get_modifiers().add, meta:get_modifiers(), invalid))
		assert(not pcall(meta:get_modifiers().remove, meta:get_modifiers(), invalid))
		assert(#meta:get_modifiers():get_list() == 0)
	end

	meta:set_string("keep", "value")
	assert(meta:get_modifiers():set_list({first, second, first, unregistered}))
	names = meta:get_modifiers():get_list()
	assert(#names == 3 and names[1] == second and names[2] == first and names[3] == unregistered)
	for _, invalid in ipairs({{first, "invalid"}, {first, 42}, "invalid", false}) do
		assert(not pcall(meta:get_modifiers().set_list, meta:get_modifiers(), invalid))
		assert(#meta:get_modifiers():get_list() == 3)
		assert(meta:get_modifiers():get_list()[3] == unregistered)
	end
	assert(not pcall(meta:get_modifiers().set_list, meta:get_modifiers(), nil))
	assert(#meta:get_modifiers():get_list() == 3)
	assert(meta:get_string("keep") == "value")
	assert(meta:get_modifiers():set_list({first}))
	assert(#meta:get_modifiers():get_list() == 1)
	assert(meta:get_modifiers():set_list({}))
	assert(#meta:get_modifiers():get_list() == 0)

	assert(meta:from_table({
		fields = { name = "value" },
		modifiers = {first, second, first, unregistered}
	}))
	local values = meta:to_table()
	assert(#values.modifiers == 3)
	assert(values.modifiers[1] == second and values.modifiers[2] == first
			and values.modifiers[3] == unregistered)
	assert(values.fields.name == "value")
	-- Check that the returned table is a copy, and modifying it won't change actual modifiers
	values.modifiers[1] = first
	assert(meta:get_modifiers():get_list()[1] == second)
	-- Removing the last ordinary field must not remove the modifiers.
	meta:set_string("name", "")
	assert(#meta:get_modifiers():get_list() == 3)
	assert(meta:from_table({modifiers = {first}}))
	assert(#meta:to_table().modifiers == 1)
	assert(meta:from_table({fields = {name = "value"}}))
	assert(#meta:get_modifiers():get_list() == 0)
	assert(meta:get_modifiers():add(first))
	assert(meta:from_table({modifiers = {}}))
	assert(#meta:get_modifiers():get_list() == 0)
	assert(#meta:to_table().fields == 0)
	assert(meta:get_modifiers():add(first))
	assert(meta:from_table(nil))
	assert(#meta:to_table().modifiers == 0)
	assert(not pcall(meta.from_table, meta, {modifiers = {""}}))
	assert(not pcall(meta.from_table, meta, {modifiers = {42}}))
	assert(not pcall(meta.from_table, meta, {modifiers = "invalid"}))
	assert(not pcall(meta.from_table, meta, {modifiers = 50}))
	assert(not pcall(meta.from_table, meta, {modifiers = core.get_meta}))
	assert(not pcall(meta.from_table, meta, {modifiers = meta}))
	assert(#meta:to_table().modifiers == 0)
end

unittests.register("test_item_modifier_methods", function()
	local stack = ItemStack("unittests:iron_lump")

	test_modifier_methods(stack:get_meta())

	assert(stack:get_meta():get_modifiers():add("unittests:modifier_first"))
	local copy = ItemStack(stack:to_string())
	assert(copy:get_meta():get_modifiers():get_list()[1] == "unittests:modifier_first")
end)

unittests.register("test_node_modifier_methods", function(player, pos)
	core.set_node(pos, {name = "air"})
	local meta = core.get_meta(pos)

	test_modifier_methods(meta)

	meta:get_inventory():set_size("main", 1)
	meta:get_inventory():set_stack("main", 1, "unittests:description_test")
	assert(meta:get_modifiers():add("unittests:modifier_first"))
	assert(meta:from_table(meta:to_table()))
	assert(meta:get_modifiers():get_list()[1] == "unittests:modifier_first")
	meta:get_modifiers():remove("unittests:modifier_first")
	assert(meta:get_inventory():get_stack("main", 1):get_name() ==
		"unittests:description_test")
	meta:from_table(nil)
end, {map = true})

core.register_node("unittests:modifier_transfer", {
	description = "Modifier transfer test node",
	groups = {dig_immediate = 3},
	on_construct = function(pos)
		local meta = core.get_meta(pos)
		meta:set_string("private_field", "preserved")
		meta:mark_as_private("private_field")
		meta:get_inventory():set_size("main", 1)
	end,
	preserve_metadata = function(pos, node, oldmeta, drops)
		drops[1]:get_meta():set_string("first_at_preserve",
			drops[1]:get_meta():get_modifiers():get_list()[1] or "")
		-- The callback can override the default transfer.
		drops[1]:get_meta():get_modifiers():remove("unittests:modifier_second")
	end,
	after_place_node = function(pos)
		local meta = core.get_meta(pos)
		meta:set_string("first_at_place", meta:get_modifiers():get_list()[1] or "")
	end,
})

unittests.register("test_modifier_transfer", function(player, pos)
	local first, second = "unittests:modifier_first", "unittests:modifier_second"
	local unknown = "unittests:missing_transfer_modifier"
	core.set_node(pos, {name = "unittests:modifier_transfer"})
	assert(core.get_meta(pos):from_table({modifiers = {first, second, unknown}}))
	local drops
	local handle_drops = core.handle_node_drops
	core.handle_node_drops = function(_, items)
		drops = items
	end
	local ok, err = pcall(core.node_dig, pos, core.get_node(pos), nil)
	core.handle_node_drops = handle_drops
	assert(ok, err)
	assert(core.get_node(pos).name == "air")
	assert(#core.get_meta(pos):get_modifiers():get_list() == 0)
	local stack = ItemStack(drops[1])
	assert(stack:get_meta():get_string("first_at_preserve") == first)
	local names = stack:get_meta():get_modifiers():get_list()
	assert(#names == 2 and names[1] == first and names[2] == unknown)
	local remaining, placed = core.item_place_node(stack, nil, {
		type = "node", under = pos, above = vector.offset(pos, 0, 1, 0),
	})
	assert(placed and vector.equals(placed, pos))
	assert(remaining:is_empty())
	local meta = core.get_meta(pos)
	names = meta:get_modifiers():get_list()
	assert(#names == 2 and names[1] == first and names[2] == unknown)
	assert(meta:get_string("first_at_place") == first)
	assert(meta:get_string("private_field") == "preserved")
	assert(meta:get_inventory():get_size("main") == 1)
	core.swap_node(pos, {name = "unittests:modifier_transfer"})
	assert(#meta:get_modifiers():get_list() == 2)
	core.set_node(pos, {name = "air"})
	assert(#meta:get_modifiers():get_list() == 0)
end, {map = true})

core.register_node("unittests:modifier_transformed_drop", {
	drop = "unittests:description_test",
})
core.register_node("unittests:modifier_selected_drops", {
	groups = {attached_node = 1},
	drop = {
		items = {
			{items = {"unittests:description_test"}},
			{items = {"unittests:modifier_transformed_drop"}},
		},
	},
	preserve_node_modifiers = function(pos, node, modifiers, drops)
		assert(#drops == 2)
		assert(#drops[1]:get_meta():get_modifiers():get_list() == 0)
		assert(drops[2]:get_meta():get_modifiers():set_list(modifiers))
		-- Callback arguments must not alias the caller's position/node.
		pos.x = pos.x + 100
		node.name = "air"
	end,
})
core.register_node("unittests:modifier_discard_drops", {
	preserve_node_modifiers = function() end,
})

core.register_node("unittests:modifier_explicit_drop", {
	drop = "unittests:modifier_explicit_drop",
})
core.register_node("unittests:modifier_table_drop", {
	drop = {
		items = {
			{items = {"unittests:modifier_table_drop"}},
			{items = {"unittests:modifier_table_drop", "unittests:description_test"},
				inherit_node_modifiers = true},
		},
	},
})

unittests.register("test_modifier_drop_selection", function(player, pos)
	local modifier = "unittests:modifier_first"
	local function dig(name)
		core.set_node(pos, {name = name})
		assert(core.get_meta(pos):get_modifiers():add(modifier))
		local drops
		local original = core.handle_node_drops
		core.handle_node_drops = function(_, items) drops = items end
		local ok, err = pcall(core.node_dig, pos, core.get_node(pos), nil)
		core.handle_node_drops = original
		assert(ok, err)
		assert(core.get_node(pos).name == "air")
		return drops
	end
	local drops = dig("unittests:modifier_explicit_drop")
	assert(#ItemStack(drops[1]):get_meta():get_modifiers():get_list() == 0)
	drops = dig("unittests:modifier_table_drop")
	assert(#drops == 3)
	assert(#ItemStack(drops[1]):get_meta():get_modifiers():get_list() == 0)
	assert(ItemStack(drops[2]):get_meta():get_modifiers():get_list()[1] == modifier)
	assert(ItemStack(drops[3]):get_meta():get_modifiers():get_list()[1] == modifier)
	drops = dig("unittests:modifier_transformed_drop")
	assert(#ItemStack(drops[1]):get_meta():get_modifiers():get_list() == 0)
	drops = dig("unittests:modifier_selected_drops")
	assert(#ItemStack(drops[1]):get_meta():get_modifiers():get_list() == 0)
	assert(ItemStack(drops[2]):get_meta():get_modifiers():get_list()[1] == modifier)
	drops = dig("unittests:modifier_discard_drops")
	assert(#ItemStack(drops[1]):get_meta():get_modifiers():get_list() == 0)

	core.set_node(vector.offset(pos, 0, -1, 0), {name = "air"})
	core.set_node(pos, {name = "unittests:modifier_selected_drops"})
	assert(core.get_meta(pos):get_modifiers():add(modifier))
	local spawned = {}
	local original = core.add_item
	core.add_item = function(_, item) spawned[#spawned + 1] = ItemStack(item) end
	local ok, err = pcall(core.check_single_for_falling, pos)
	core.add_item = original
	assert(ok, err)
	assert(#spawned == 2)
	assert(#spawned[1]:get_meta():get_modifiers():get_list() == 0)
	assert(spawned[2]:get_meta():get_modifiers():get_list()[1] == modifier)
	assert(core.get_node(pos).name == "air")
end, {map = true})

core.register_node("unittests:modifier_leveled", {
	drawtype = "nodebox",
	paramtype2 = "leveled",
	leveled = 16,
	node_box = {type = "leveled", fixed = {-0.5, -0.5, -0.5, 0.5, 0, 0.5}},
})

unittests.register("test_modifier_falling_transfer", function(player, pos)
	local first = "unittests:modifier_first"
	local unknown = "unittests:falling_unknown"
	local names = {first, unknown}
	local function check_names(actual)
		assert(#actual == 2 and actual[1] == first and actual[2] == unknown)
	end

	-- Node -> entity -> static data -> entity -> landed node.
	core.set_node(pos, {name = "unittests:modifier_transformed_drop"})
	assert(core.get_meta(pos):get_modifiers():set_list(names))
	local ok, object = core.spawn_falling_node(pos)
	assert(ok and object)
	assert(#core.get_meta(pos):get_modifiers():get_list() == 0)
	local data = object:get_luaentity():get_staticdata()
	object:remove()
	object = core.add_entity(pos, "__builtin:falling_node", data)
	assert(object)
	local entity = object:get_luaentity()
	check_names(entity.meta.modifiers)
	assert(entity:try_place(pos, core.get_node(pos)))
	check_names(core.get_meta(pos):get_modifiers():get_list())
	object:remove()

	-- Failure must use the saved modifiers, not those at the entity's position.
	local function break_into_drops(name, modifiers)
		core.set_node(pos, {name = "air"})
		assert(core.get_meta(pos):get_modifiers():set_list({"unittests:modifier_second"}))
		local saved = core.serialize({node = {name = name}, meta = {modifiers = modifiers}})
		local obj = core.add_entity(pos, "__builtin:falling_node", saved)
		assert(obj)
		local support = vector.offset(pos, 2, -1, 0)
		core.set_node(support, {name = "unittests:modifier_transformed_drop"})
		local drops = {}
		local add_item = core.add_item
		core.add_item = function(_, item) drops[#drops + 1] = ItemStack(item) end
		local success, err = pcall(obj:get_luaentity().on_step, obj:get_luaentity(), 0, {
			collides = true, touching_ground = true,
			collisions = {{type = "node", axis = "y", node_pos = support}},
		})
		core.add_item = add_item
		obj:remove()
		core.remove_node(support)
		assert(success, err)
		return drops
	end
	local drops = break_into_drops("unittests:modifier_leveled", names)
	check_names(drops[1]:get_meta():get_modifiers():get_list())
	drops = break_into_drops("unittests:modifier_transformed_drop", names)
	assert(#drops[1]:get_meta():get_modifiers():get_list() == 0)
	drops = break_into_drops("unittests:modifier_table_drop", names)
	assert(#drops == 3 and #drops[1]:get_meta():get_modifiers():get_list() == 0)
	check_names(drops[2]:get_meta():get_modifiers():get_list())
	check_names(drops[3]:get_meta():get_modifiers():get_list())
	drops = break_into_drops("unittests:modifier_selected_drops", names)
	assert(#drops == 2 and #drops[1]:get_meta():get_modifiers():get_list() == 0)
	check_names(drops[2]:get_meta():get_modifiers():get_list())
	drops = break_into_drops("unittests:modifier_leveled", {})
	assert(#drops[1]:get_meta():get_modifiers():get_list() == 0)

	-- Incompatible leveled nodes must not merge or lose either modifier list.
	core.set_node(pos, {name = "unittests:modifier_leveled"})
	assert(core.get_meta(pos):get_modifiers():set_list(names))
	object = core.add_entity(pos, "__builtin:falling_node", core.serialize({
		node = {name = "unittests:modifier_leveled", level = 16},
		meta = {modifiers = {unknown, first}},
	}))
	entity = object:get_luaentity()
	local level = core.get_node_level(pos)
	assert(not entity:try_place(pos, core.get_node(pos)))
	assert(core.get_node_level(pos) == level)
	check_names(core.get_meta(pos):get_modifiers():get_list())
	entity.meta.modifiers = names
	assert(entity:try_place(pos, core.get_node(pos)))
	assert(core.get_node_level(pos) > level)
	check_names(core.get_meta(pos):get_modifiers():get_list())
	object:remove()
	core.remove_node(pos)
end, {map = true})

core.register_node_modifier("unittests:light_bright", {light_source = 14})
core.register_node_modifier("unittests:light_off", {light_source = 0})

unittests.register("test_modifier_lighting", function(player, pos)
	local source = vector.offset(pos, 8, 8, 8)
	local neighbor = vector.offset(source, 1, 0, 0)
	-- Enclose the test in opaque nodes so both light banks start dark.
	for z = -2, 2 do
	for y = -2, 2 do
	for x = -2, 2 do
		core.set_node(vector.offset(source, x, y, z),
			{name = "unittests:modifier_transformed_drop"})
	end end end
	core.set_node(neighbor, {name = "air"})
	local meta = core.get_meta(source)
	assert(meta:get_modifiers():add("unittests:light_bright"))
	assert(core.get_node_light(source, 0) == 14)
	assert(core.get_node_light(neighbor, 0) == 13)
	assert(meta:get_modifiers():set_list({"unittests:light_off"}))
	assert(core.get_node_light(source, 0) == 0)
	assert(core.get_node_light(neighbor, 0) == 0)
	assert(meta:get_modifiers():add("unittests:light_bright"))
	meta:get_modifiers():remove("unittests:light_bright")
	assert(core.get_node_light(neighbor, 0) == 0)
	assert(meta:from_table({modifiers = {"unittests:light_bright"}}))
	assert(core.get_node_light(neighbor, 0) == 13)
	assert(meta:from_table(nil))
	assert(core.get_node_light(neighbor, 0) == 0)
	for z = -2, 2 do
	for y = -2, 2 do
	for x = -2, 2 do
		core.remove_node(vector.offset(source, x, y, z))
	end end end
end, {map = true})

local function check_failed_metadata_import(meta)
	assert(meta:from_table({fields = {keep = "original"}, modifiers = {first, second}}))
	local inventory = meta.get_inventory and meta:get_inventory()
	if inventory then
		inventory:set_size("main", 1)
		inventory:set_stack("main", 1, "unittests:iron_lump 3")
		meta:mark_as_private("keep")
	end
	local too_many = {}
	for i = 1, 17 do
		too_many[i] = "unittests:import_limit_" .. i
	end
	for _, modifiers in ipairs({{"invalid"}, {42}, "invalid", too_many}) do
		local ok, result = pcall(meta.from_table, meta, {
			fields = {keep = "replaced", extra = "unexpected"},
			inventory = {main = {}},
			modifiers = modifiers,
		})
		if modifiers == too_many then
			assert(ok and result == false)
		else
			assert(not ok and tostring(result):lower():find("modifier"))
		end
		assert(meta:get_string("keep") == "original")
		assert(meta:get_string("extra") == "")
		local names = meta:get_modifiers():get_list()
		assert(#names == 2 and names[1] == first and names[2] == second)
		if inventory then
			assert(inventory:get_size("main") == 1)
			assert(inventory:get_stack("main", 1):to_string() == "unittests:iron_lump 3")
		end
	end
	-- Successful replacement still removes omitted fields, inventory and modifiers.
	assert(meta:from_table({fields = {new = "value"}}))
	assert(meta:get_string("keep") == "")
	assert(meta:get_string("new") == "value")
	assert(#meta:get_modifiers():get_list() == 0)
	if inventory then
		assert(inventory:get_size("main") == 0)
	end
end

unittests.register("test_item_metadata_import_atomic", function()
	local stack = ItemStack("unittests:iron_lump")
	check_failed_metadata_import(stack:get_meta())
end)

unittests.register("test_node_metadata_import_atomic", function(_, pos)
	core.set_node(pos, {name = "air"})
	check_failed_metadata_import(core.get_meta(pos))
	core.get_meta(pos):from_table(nil)
end, {map = true})

core.register_node_modifier("unittests:modifier_empty", {})
unittests.register("test_item_modifier_pruning", function()
	local stack = ItemStack("unittests:iron_lump")
	local meta = stack:get_meta()
	local empty = "unittests:modifier_empty"
	assert(meta:get_modifiers():set_list({first, empty, unregistered}))
	assert(meta:get_modifiers():add(second))
	assert(#meta:get_modifiers():get_list() == 4) -- Default preserves earlier overrides.
	assert(meta:get_modifiers():add(second, true))
	local names = meta:get_modifiers():get_list()
	assert(#names == 3 and names[1] == empty and names[2] == unregistered
		and names[3] == second)
	assert(meta:get_modifiers():add(first, true))
	names = meta:get_modifiers():get_list()
	assert(#names == 3 and names[3] == first)

	local full = {}
	for i = 1, 15 do full[i] = "unittests:unknown_prune_" .. i end
	full[16] = first
	assert(meta:get_modifiers():set_list(full))
	assert(meta:get_modifiers():add(second, true)) -- Prune before checking capacity.
	names = meta:get_modifiers():get_list()
	assert(#names == 16 and names[16] == second)
	assert(not meta:get_modifiers():add(empty, true))
	assert(not meta:get_modifiers():add("unittests:missing_prune", true))
	names = meta:get_modifiers():get_list()
	assert(#names == 16 and names[16] == second)
	for i = 1, 15 do assert(names[i] == full[i]) end
end)

core.register_node("unittests:modifier_falling_leveled", {
	drawtype = "nodebox",
	paramtype2 = "leveled",
	leveled = 16,
	groups = {falling_node = 1},
	node_box = {type = "leveled", fixed = {-0.5, -0.5, -0.5, 0.5, 0, 0.5}},
})

unittests.register("test_modifier_falling_start", function(_, pos)
	local above = vector.offset(pos, 0, 1, 0)
	local name = "unittests:modifier_falling_leveled"
	core.set_node(pos, {name = name})
	core.set_node(above, {name = name})
	assert(core.get_meta(pos):get_modifiers():set_list({first, second}))
	for _, modifiers in ipairs({{}, {first}, {second, first}}) do
		assert(core.get_meta(above):get_modifiers():set_list(modifiers))
		assert(not core.check_single_for_falling(above))
		assert(core.get_node(above).name == name)
	end
	assert(core.get_meta(above):get_modifiers():set_list({first, second}))
	assert(core.check_single_for_falling(above))
	assert(core.get_node(above).name == "air")
	local object
	for obj in core.objects_inside_radius(above, 1) do
		local entity = obj:get_luaentity()
		if entity and entity.name == "__builtin:falling_node" then
			object = obj
			break
		end
	end
	assert(object)
	local entity = object:get_luaentity()
	-- Change the support after the entity was created: landing must recheck.
	local level = core.get_node_level(pos)
	assert(core.get_meta(pos):get_modifiers():set_list({second, first}))
	assert(not entity:try_place(pos, core.get_node(pos)))
	assert(core.get_node_level(pos) == level)
	assert(core.get_meta(pos):get_modifiers():set_list({first, second}))
	assert(entity:try_place(pos, core.get_node(pos)))
	assert(core.get_node_level(pos) > level)
	object:remove()
	core.remove_node(above)
	core.remove_node(pos)
end, {map = true})

unittests.register("test_item_modifier_reference", function()
	local stack = ItemStack("unittests:iron_lump")
	local meta = stack:get_meta()
	local modifiers = meta:get_modifiers()
	assert(meta.add_modifier == nil and meta.set_modifiers == nil)
	assert(modifiers:add(first))
	assert(meta:to_table().modifiers[1] == first)
	assert(meta:from_table({modifiers = {second}}))
	assert(modifiers:get_list()[1] == second)
	local copy = modifiers:get_list()
	copy[1] = first
	assert(modifiers:get_list()[1] == second)
	meta, stack = nil, nil
	collectgarbage("collect")
	assert(modifiers:add(first))
	assert(#modifiers:get_list() == 2)
	assert(modifiers:set_list({}))
	assert(#modifiers:get_list() == 0)
end)

unittests.register("test_node_modifier_reference", function(_, pos)
	core.set_node(pos, {name = "air"})
	local meta = core.get_meta(pos)
	local modifiers = meta:get_modifiers()
	assert(meta.add_modifier == nil and meta.set_modifiers == nil)
	assert(#core.find_nodes_with_meta(pos, pos) == 0)
	meta = nil
	collectgarbage("collect")
	assert(modifiers:add(first))
	assert(core.get_meta(pos):to_table().modifiers[1] == first)
	core.set_node(pos, {name = "air"})
	assert(#modifiers:get_list() == 0)
	assert(modifiers:set_list({second}))
	assert(core.get_meta(pos):get_modifiers():get_list()[1] == second)
	assert(modifiers:set_list({}))
end, {map = true})

unittests.register("test_modifier_metadata_equality", function(_, pos)
	core.set_node(pos, {name = "air"})
	local node = core.get_meta(pos)
	local stack = ItemStack("unittests:iron_lump")
	local other = ItemStack(stack)
	local item = stack:get_meta()
	node:set_string("field", "value")
	item:set_string("field", "value")
	other:get_meta():set_string("field", "value")
	assert(item:get_modifiers():add(first))
	assert(node:get_modifiers():add(second))
	node:get_inventory():set_size("main", 1)
	node:get_inventory():set_stack("main", 1, "unittests:iron_lump")
	assert(node:equals(item) and item:equals(node))
	assert(item:equals(other:get_meta()) and other:get_meta():equals(item))
	assert(not stack:equals(other))
	assert(other:get_meta():get_modifiers():add(first))
	assert(stack:equals(other))
	item:set_string("field", "different")
	assert(not node:equals(item) and not item:equals(node))
	core.set_node(pos, {name = "air"})
end, {map = true})

-- Independent visual properties and partial-face masks.
core.register_node_modifier("unittests:tiles_all", {tiles = {"testnodes_allfaces.png"}})
core.register_node_modifier("unittests:tiles_top", {tiles = {"testnodes_allfaces.png", ""}})
core.register_node_modifier("unittests:tiles_sides", {
	tiles = {"", {name = "testnodes_allfaces.png", scale = 2, align_style = "world"}},
})
core.register_node_modifier("unittests:alpha_blend", {use_texture_alpha = "blend"})
core.register_node_modifier("unittests:alpha_clip", {use_texture_alpha = "clip"})
core.register_node_modifier("unittests:alpha_opaque", {use_texture_alpha = "opaque"})

for i, definition in ipairs({
	{tiles = "invalid"},
	{use_texture_alpha = true},
}) do
	assert(not pcall(core.register_node_modifier, "unittests:invalid_visual_" .. i, definition),
		"Invalid node modifier visual definition was accepted: " .. core.serialize(definition))
end

unittests.register("test_modifier_visual_properties", function()
	local modifiers = ItemStack("unittests:iron_lump"):get_meta():get_modifiers()
	assert(modifiers:add("unittests:tiles_all"))
	assert(modifiers:add("unittests:alpha_blend", true))
	assert(#modifiers:get_list() == 2)
	assert(modifiers:add("unittests:tiles_top", true))
	assert(#modifiers:get_list() == 3)
	assert(modifiers:add("unittests:tiles_sides", true))
	assert(table.concat(modifiers:get_list(), ",") ==
		"unittests:alpha_blend,unittests:tiles_top,unittests:tiles_sides")
	assert(modifiers:add("unittests:alpha_clip", true))
	assert(table.concat(modifiers:get_list(), ",") ==
		"unittests:tiles_top,unittests:tiles_sides,unittests:alpha_clip")
end)
