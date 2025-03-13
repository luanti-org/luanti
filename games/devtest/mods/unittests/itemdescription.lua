local full_description = "Description Test Item\nFor testing item decription"
core.register_tool("unittests:description_test", {
	description = full_description,
	inventory_image = "unittests_description_test.png",
	groups = { dummy = 1 },
})

core.register_chatcommand("item_description", {
	param = "",
	description = "Show the short and full description of the wielded item.",
	func = function(name)
		local player = core.get_player_by_name(name)
		local item = player:get_wielded_item()
		return true, string.format("short_description: %s\ndescription: %s",
				item:get_short_description(), item:get_description())
	end
})

local function test_short_desc()
	local function get_short_description(item)
		return ItemStack(item):get_short_description()
	end

	local stack = ItemStack("unittests:description_test")
	assert(stack:get_short_description() == "Description Test Item")
	assert(get_short_description("unittests:description_test") == "Description Test Item")
	assert(core.registered_items["unittests:description_test"].short_description == nil)
	assert(stack:get_description() == full_description)
	assert(stack:get_description() == core.registered_items["unittests:description_test"].description)

	stack:get_meta():set_string("description", "Hello World")
	assert(stack:get_short_description() == "Hello World")
	assert(stack:get_description() == "Hello World")
	assert(get_short_description(stack) == "Hello World")
	assert(get_short_description("unittests:description_test") == "Description Test Item")

	stack:get_meta():set_string("short_description", "Foo Bar")
	assert(stack:get_short_description() == "Foo Bar")
	assert(stack:get_description() == "Hello World")

	return true
end
unittests.register("test_short_desc", test_short_desc)
