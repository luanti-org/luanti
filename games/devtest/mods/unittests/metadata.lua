-- Tests of generic and specific metadata functionality

local compare_meta = ItemStack("unittests:iron_lump"):get_meta()
compare_meta:from_table({
	fields = {
		a = "1",
		b = "2",
		c = "3",
		d = "4",
		e = "e",
		["0.3"] = "0.29999999999999999",
		["0.1+0.2"] = "0.30000000000000004",
	},
})

local function test_metadata(meta)
	meta:from_table({fields = {a = 2, b = "2"}})
	meta:set_string("a", 1)
	meta:set_string("c", "3")
	meta:set_int("d", 4)
	meta:set_string("e", "e")

	meta:set_string("", "!")
	meta:set_string("", "")

	meta:set_float("0.3", 0.3)
	meta:set_float("0.1+0.2", 0.1 + 0.2)

	assert(meta:equals(compare_meta))

	local tab = meta:to_table()
	assert(tab.fields.a == "1")
	assert(tab.fields.b == "2")
	assert(tab.fields.c == "3")
	assert(tab.fields.d == "4")
	assert(tab.fields.e == "e")
	assert(tab.fields["0.3"] == "0.29999999999999999")
	assert(tab.fields["0.1+0.2"] == "0.30000000000000004")

	local keys = meta:get_keys()
	assert(table.indexof(keys, "a") > 0)
	assert(table.indexof(keys, "b") > 0)
	assert(table.indexof(keys, "c") > 0)
	assert(table.indexof(keys, "d") > 0)
	assert(table.indexof(keys, "e") > 0)
	assert(#keys == 7)

	assert(not meta:contains(""))
	assert(meta:contains("a"))
	assert(meta:contains("b"))
	assert(meta:contains("c"))
	assert(meta:contains("d"))
	assert(meta:contains("e"))

	assert(meta:get("") == nil)
	assert(meta:get_string("") == "")
	assert(meta:get_int("") == 0)
	assert(meta:get_float("") == 0.0)
	assert(meta:get("a") == "1")
	assert(meta:get_string("a") == "1")
	assert(meta:get_int("a") == 1)
	assert(meta:get_float("a") == 1.0)
	assert(meta:get_int("e") == 0)
	assert(meta:get_float("e") == 0.0)
	assert(meta:get_float("0.3") == 0.3)
	assert(meta:get_float("0.1+0.2") == 0.1 + 0.2)

	meta:set_float("f", 1.1)
	meta:set_string("g", "${f}")
	meta:set_string("h", "${g}")
	meta:set_string("i", "${h}")
	assert(meta:get_float("h") > 1)
	assert(meta:get_string("i") == "${f}")

	meta:set_float("j", 1.23456789)
	assert(meta:get_float("j") == 1.23456789)
	meta:set_float("j", -1 / 0)
	assert(meta:get_float("j") == -1 / 0)
	meta:set_float("j", 0 / 0)
	assert(core.is_nan(meta:get_float("j")))

	meta:from_table()
	assert(next(meta:to_table().fields) == nil)
	assert(#meta:get_keys() == 0)

	assert(not meta:equals(compare_meta))
end

local function test_metadata_compat(meta)
	-- key/value removal using set_string (undocumented, deprecated way)
	meta:set_string("key", "value")
	assert(meta:get_string("key") == "value")
	meta:set_string("key", nil) -- ignore warning
	assert(meta:to_table().fields["key"] == nil)

	-- undocumented but supported consequence of Lua's
	-- automatic string <--> number cast
	meta:set_string("key", 2)
	assert(meta:get_string("key") == "2")

	-- from_table with non-string keys (supported)
	local values = meta:to_table()
	values.fields["new"] = 420
	meta:from_table(values)
	assert(meta:get_int("new") == 420)
	values.fields["new"] = nil
	meta:from_table(values)
	assert(meta:get("new") == nil)
end


local storage_a = core.get_mod_storage()
local storage_b = core.get_mod_storage()
local function test_mod_storage()
	assert(rawequal(storage_a, storage_b))
	test_metadata(storage_a)
end
unittests.register("test_mod_storage", test_mod_storage)

local function test_item_metadata()
	local meta = ItemStack("unittest:coal_lump"):get_meta()
	test_metadata(meta)
	test_metadata_compat(meta)
end
unittests.register("test_item_metadata", test_item_metadata)

local function test_node_metadata(player, pos)
	test_metadata(core.get_meta(pos))
end
unittests.register("test_node_metadata", test_node_metadata, {map=true})

local function get_cracky_cap(item)
	local value = item:get_tool_capabilities()
	assert(type(value) == "table")
	value = value.groupcaps
	assert(type(value) == "table")
	value = value.cracky
	assert(type(value) == "table")
	value = value.times
	assert(type(value) == "table")
	value = value[1]
	assert(type(value) == "number")
	return value
end

local function test_item_metadata_tool_capabilities()
	local test_caps = {
		groupcaps={
			cracky={times={123}},
		},
	}

	-- has no tool capabilities
	local item = ItemStack("unittests:stick")
	local item_meta = item:get_meta()
	assert(dump(item:get_tool_capabilities()) == dump(ItemStack(""):get_tool_capabilities()))
	item_meta:set_tool_capabilities(test_caps)
	-- Can't directly compare the tables, because the pushback to Lua from get_tool_capabilities()
	-- adds values to left out fields of the tool capabilities table.
	assert(get_cracky_cap(item) == 123)

	-- has preexisting tool capabilities in its definition table
	item = ItemStack("unittests:unrepairable_tool")
	item_meta = item:get_meta()
	assert(get_cracky_cap(item) == 3)
	item_meta:set_tool_capabilities(test_caps)
	assert(get_cracky_cap(item) == 123)
end
unittests.register("test_item_metadata_tool_capabilities", test_item_metadata_tool_capabilities)

-- Applied modifiers are separate from ordinary metadata fields.
core.register_node_modifier("unittests:modifier_first", {light_source = 1})
core.register_node_modifier("unittests:modifier_second", {light_source = 2})

local function test_modifier_methods(meta)
	local first, second = "unittests:modifier_first", "unittests:modifier_second"
	assert(not meta:add_modifier("unittests:unregistered_modifier"))
	assert(not meta:add_modifier(""))
	meta:remove_modifier(first)
	assert(#meta:get_modifiers_list() == 0)
	assert(meta:add_modifier(first))
	assert(meta:add_modifier(second))
	assert(meta:add_modifier(first))
	local names = meta:get_modifiers_list()
	assert(#names == 2 and names[1] == second and names[2] == first)
	names[1] = "changed" -- The returned table is a copy.
	assert(meta:get_modifiers_list()[1] == second)
	meta:set_string("modifiers", "ordinary field")
	meta:remove_modifier(second)
	names = meta:get_modifiers_list()
	assert(#names == 1 and names[1] == first)
	meta:remove_modifier(second)
	names = meta:get_modifiers_list()
	assert(#names == 1 and names[1] == first)
	meta:remove_modifier(first)
	assert(#meta:get_modifiers_list() == 0)
	assert(meta:get_string("modifiers") == "ordinary field")
end

unittests.register("test_item_modifier_methods", function()
	local stack = ItemStack("unittests:iron_lump")
	test_modifier_methods(stack:get_meta())
	assert(stack:get_meta():add_modifier("unittests:modifier_first"))
	local copy = ItemStack(stack:to_string())
	assert(copy:get_meta():get_modifiers_list()[1] == "unittests:modifier_first")
end)

unittests.register("test_node_modifier_methods", function(player, pos)
	core.set_node(pos, {name = "air"})
	local meta = core.get_meta(pos)
	test_modifier_methods(meta)
end, {map = true})

local function test_set_modifiers(meta)
	local first, second = "unittests:modifier_first", "unittests:modifier_second"
	local unknown = "unittests:bulk_unknown"
	meta:set_string("keep", "value")
	assert(meta:set_modifiers({first, second, first, unknown}))
	local names = meta:get_modifiers_list()
	assert(#names == 3 and names[1] == second and names[2] == first and names[3] == unknown)
	for _, invalid in ipairs({{first, "invalid"}, {first, 42}, "invalid", false}) do
		assert(not meta:set_modifiers(invalid))
		assert(#meta:get_modifiers_list() == 3)
		assert(meta:get_modifiers_list()[3] == unknown)
	end
	assert(not meta:set_modifiers(nil))
	assert(meta:get_string("keep") == "value")
	assert(meta:set_modifiers({first}))
	assert(#meta:get_modifiers_list() == 1)
	assert(meta:set_modifiers({}))
	assert(#meta:get_modifiers_list() == 0)
end

local function test_modifier_tables(meta)
	test_set_modifiers(meta)
	local first, second = "unittests:modifier_first", "unittests:modifier_second"
	local unknown = "unittests:missing_modifier"
	assert(meta:from_table({fields = {modifiers = "ordinary field"},
		modifiers = {first, second, first, unknown}}))
	local values = meta:to_table()
	assert(values.fields.modifiers == "ordinary field")
	assert(#values.modifiers == 3)
	assert(values.modifiers[1] == second and values.modifiers[2] == first)
	assert(values.modifiers[3] == unknown)
	assert(meta:from_table(values))
	assert(meta:get_modifiers_list()[3] == unknown)
	values.modifiers[1] = first
	assert(meta:get_modifiers_list()[1] == second)
	-- Removing the last ordinary field must not remove the modifiers.
	meta:set_string("modifiers", "")
	assert(#meta:get_modifiers_list() == 3)
	assert(meta:from_table({modifiers = {first}}))
	assert(#meta:to_table().modifiers == 1)
	assert(meta:from_table({fields = {}}))
	assert(#meta:get_modifiers_list() == 0)
	assert(meta:add_modifier(first))
	assert(meta:from_table({modifiers = {}}))
	assert(#meta:get_modifiers_list() == 0)
	assert(meta:add_modifier(first))
	assert(meta:from_table(nil))
	assert(#meta:to_table().modifiers == 0)
	assert(not meta:from_table({modifiers = {""}}))
	assert(not meta:from_table({modifiers = {42}}))
	assert(not meta:from_table({modifiers = "invalid"}))
end

unittests.register("test_item_modifier_tables", function()
	local stack = ItemStack("unittests:description_test")
	test_modifier_tables(stack:get_meta())
end)

unittests.register("test_node_modifier_tables", function(player, pos)
	core.set_node(pos, {name = "air"})
	local meta = core.get_meta(pos)
	test_modifier_tables(meta)
	meta:get_inventory():set_size("main", 1)
	meta:get_inventory():set_stack("main", 1, "unittests:description_test")
	assert(meta:add_modifier("unittests:modifier_first"))
	assert(meta:from_table(meta:to_table()))
	assert(meta:get_inventory():get_stack("main", 1):get_name() ==
		"unittests:description_test")
	assert(meta:get_modifiers_list()[1] == "unittests:modifier_first")
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
			drops[1]:get_meta():get_modifiers_list()[1] or "")
		-- The callback can override the default transfer.
		drops[1]:get_meta():remove_modifier("unittests:modifier_second")
	end,
	after_place_node = function(pos)
		local meta = core.get_meta(pos)
		meta:set_string("first_at_place", meta:get_modifiers_list()[1] or "")
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
	assert(#core.get_meta(pos):get_modifiers_list() == 0)
	local stack = ItemStack(drops[1])
	assert(stack:get_meta():get_string("first_at_preserve") == first)
	local names = stack:get_meta():get_modifiers_list()
	assert(#names == 2 and names[1] == first and names[2] == unknown)
	local remaining, placed = core.item_place_node(stack, nil, {
		type = "node", under = pos, above = vector.offset(pos, 0, 1, 0),
	})
	assert(placed and vector.equals(placed, pos))
	assert(remaining:is_empty())
	local meta = core.get_meta(pos)
	names = meta:get_modifiers_list()
	assert(#names == 2 and names[1] == first and names[2] == unknown)
	assert(meta:get_string("first_at_place") == first)
	assert(meta:get_string("private_field") == "preserved")
	assert(meta:get_inventory():get_size("main") == 1)
	core.swap_node(pos, {name = "unittests:modifier_transfer"})
	assert(#meta:get_modifiers_list() == 2)
	core.set_node(pos, {name = "air"})
	assert(#meta:get_modifiers_list() == 0)
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
		assert(#drops[1]:get_meta():get_modifiers_list() == 0)
		assert(drops[2]:get_meta():set_modifiers(modifiers))
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
		assert(core.get_meta(pos):add_modifier(modifier))
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
	assert(#ItemStack(drops[1]):get_meta():get_modifiers_list() == 0)
	drops = dig("unittests:modifier_table_drop")
	assert(#drops == 3)
	assert(#ItemStack(drops[1]):get_meta():get_modifiers_list() == 0)
	assert(ItemStack(drops[2]):get_meta():get_modifiers_list()[1] == modifier)
	assert(ItemStack(drops[3]):get_meta():get_modifiers_list()[1] == modifier)
	drops = dig("unittests:modifier_transformed_drop")
	assert(#ItemStack(drops[1]):get_meta():get_modifiers_list() == 0)
	drops = dig("unittests:modifier_selected_drops")
	assert(#ItemStack(drops[1]):get_meta():get_modifiers_list() == 0)
	assert(ItemStack(drops[2]):get_meta():get_modifiers_list()[1] == modifier)
	drops = dig("unittests:modifier_discard_drops")
	assert(#ItemStack(drops[1]):get_meta():get_modifiers_list() == 0)

	core.set_node(vector.offset(pos, 0, -1, 0), {name = "air"})
	core.set_node(pos, {name = "unittests:modifier_selected_drops"})
	assert(core.get_meta(pos):add_modifier(modifier))
	local spawned = {}
	local original = core.add_item
	core.add_item = function(_, item) spawned[#spawned + 1] = ItemStack(item) end
	local ok, err = pcall(core.check_single_for_falling, pos)
	core.add_item = original
	assert(ok, err)
	assert(#spawned == 2)
	assert(#spawned[1]:get_meta():get_modifiers_list() == 0)
	assert(spawned[2]:get_meta():get_modifiers_list()[1] == modifier)
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
	assert(core.get_meta(pos):set_modifiers(names))
	local ok, object = core.spawn_falling_node(pos)
	assert(ok and object)
	assert(#core.get_meta(pos):get_modifiers_list() == 0)
	local data = object:get_luaentity():get_staticdata()
	object:remove()
	object = core.add_entity(pos, "__builtin:falling_node", data)
	assert(object)
	local entity = object:get_luaentity()
	check_names(entity.meta.modifiers)
	assert(entity:try_place(pos, core.get_node(pos)))
	check_names(core.get_meta(pos):get_modifiers_list())
	object:remove()

	-- Failure must use the saved modifiers, not those at the entity's position.
	local function break_into_drops(name, modifiers)
		core.set_node(pos, {name = "air"})
		assert(core.get_meta(pos):set_modifiers({"unittests:modifier_second"}))
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
	check_names(drops[1]:get_meta():get_modifiers_list())
	drops = break_into_drops("unittests:modifier_transformed_drop", names)
	assert(#drops[1]:get_meta():get_modifiers_list() == 0)
	drops = break_into_drops("unittests:modifier_table_drop", names)
	assert(#drops == 3 and #drops[1]:get_meta():get_modifiers_list() == 0)
	check_names(drops[2]:get_meta():get_modifiers_list())
	check_names(drops[3]:get_meta():get_modifiers_list())
	drops = break_into_drops("unittests:modifier_selected_drops", names)
	assert(#drops == 2 and #drops[1]:get_meta():get_modifiers_list() == 0)
	check_names(drops[2]:get_meta():get_modifiers_list())
	drops = break_into_drops("unittests:modifier_leveled", {})
	assert(#drops[1]:get_meta():get_modifiers_list() == 0)

	-- Incompatible leveled nodes must not merge or lose either modifier list.
	core.set_node(pos, {name = "unittests:modifier_leveled"})
	assert(core.get_meta(pos):set_modifiers(names))
	object = core.add_entity(pos, "__builtin:falling_node", core.serialize({
		node = {name = "unittests:modifier_leveled", level = 16},
		meta = {modifiers = {unknown, first}},
	}))
	entity = object:get_luaentity()
	local level = core.get_node_level(pos)
	assert(not entity:try_place(pos, core.get_node(pos)))
	assert(core.get_node_level(pos) == level)
	check_names(core.get_meta(pos):get_modifiers_list())
	entity.meta.modifiers = names
	assert(entity:try_place(pos, core.get_node(pos)))
	assert(core.get_node_level(pos) > level)
	check_names(core.get_meta(pos):get_modifiers_list())
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
	assert(meta:add_modifier("unittests:light_bright"))
	assert(core.get_node_light(source, 0) == 14)
	assert(core.get_node_light(neighbor, 0) == 13)
	assert(meta:set_modifiers({"unittests:light_off"}))
	assert(core.get_node_light(source, 0) == 0)
	assert(core.get_node_light(neighbor, 0) == 0)
	assert(meta:add_modifier("unittests:light_bright"))
	meta:remove_modifier("unittests:light_bright")
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
