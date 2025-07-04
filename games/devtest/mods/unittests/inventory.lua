local function get_stack_with_meta(count)
	return ItemStack({name = "air", count = count, meta = {test = "abc"}})
end

local test_list = {
	ItemStack("air"),
	ItemStack(""),
	ItemStack(get_stack_with_meta(1)),
}

local function compare_lists(a, b)
	if not a or not b or #a ~= #b then
		return false
	end
	for i=1, #a do
		if not ItemStack(a[i]):equals(ItemStack(b[i])) then
			return false
		end
	end
	return true
end

local function test_inventory()
	local inv = core.create_detached_inventory("test")

	inv:set_lists({test = {""}})
	assert(inv:get_list("test"))

	assert(inv:get_size("test") == 1)
	assert(inv:set_size("test", 3))
	assert(not inv:set_size("test", -1))

	assert(inv:get_width("test") == 0)
	assert(inv:set_width("test", 3))
	assert(not inv:set_width("test", -1))

	inv:set_stack("test", 1, "air")
	inv:set_stack("test", 3, get_stack_with_meta(1))
	assert(not inv:is_empty("test"))
	assert(compare_lists(inv:get_list("test"), test_list))

	assert(inv:add_item("test", "air") == ItemStack())
	assert(inv:add_item("test", get_stack_with_meta(1)) == ItemStack())
	assert(inv:get_stack("test", 1) == ItemStack("air 2"))

	assert(inv:room_for_item("test", "air 99"))
	inv:set_stack("test", 2, "air 99")
	assert(not inv:room_for_item("test", "air 99"))
	inv:set_stack("test", 2, "")

	assert(inv:contains_item("test", "air"))
	assert(inv:contains_item("test", "air 4"))
	assert(not inv:contains_item("test", "air 5"))
	assert(not inv:contains_item("test", "air 99"))
	assert(inv:contains_item("test", "air 2", true))
	assert(not inv:contains_item("test", "air 3", true))
	assert(inv:contains_item("test", get_stack_with_meta(2), true))
	assert(not inv:contains_item("test", get_stack_with_meta(3), true))

	-- Items should be removed in reverse and combine with first stack removed
	assert(inv:remove_item("test", "air") == get_stack_with_meta(1))
	assert(inv:remove_item("test", "air 2") == get_stack_with_meta(2))
	assert(inv:remove_item("test", "air") == ItemStack("air"))
	assert(inv:is_empty("test"))

	inv:set_stack("test", 1, "air 3")
	inv:set_stack("test", 3, get_stack_with_meta(2))
	assert(inv:remove_item("test", "air 4", true) == ItemStack("air 3"))
	inv:set_stack("test", 1, "air 3")
	assert(inv:remove_item("test", get_stack_with_meta(3), true) == get_stack_with_meta(2))
	assert(inv:remove_item("test", "air 3", true) == ItemStack("air 3"))
	assert(inv:is_empty("test"))

	-- Failure of set_list(s) should not change inventory
	local before = inv:get_list("test")
	pcall(inv.set_lists, inv, {test = true})
	pcall(inv.set_list, inv, "test", true)
	local after = inv:get_list("test")
	assert(compare_lists(before, after))

	local location = inv:get_location()
	assert(core.remove_detached_inventory("test"))
	assert(not core.get_inventory(location))
end

unittests.register("test_inventory", test_inventory)
