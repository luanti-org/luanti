-- Test item (un)registration and overriding
do
	local itemname = "unittests:test_override_item"
	core.register_craftitem(":" .. itemname, {description = "foo"})
	assert(assert(core.registered_items[itemname]).description == "foo")
	core.override_item(itemname, {description = "bar"})
	assert(assert(core.registered_items[itemname]).description == "bar")
	core.override_item(itemname, {}, {"description"})
	-- description has the empty string as a default
	assert(assert(core.registered_items[itemname]).description == "")
	core.unregister_item("unittests:test_override_item")
	assert(core.registered_items["unittests:test_override_item"] == nil)
end

-- core.get_modnames
do
	-- Alphabetical sorting
	-- [name] = index
	local names = table.key_value_swap(core.get_modnames())
	assert(names["unittests"])
	assert(names["unittests"] > names["first_mod"])
	assert(names["unittests"] > names["basenodes"])
	assert(names["unittests"] < names["util_commands"])

	-- Technical sorting (load order)
	-- Cannot use the 'unittests' mod because its dependencies are in alphabetical order.
	local list = core.get_modnames(true)
	names = table.key_value_swap(list)
	assert(names["first_mod"] == 1)
	assert(names["last_mod"] == #list)
	assert(names["testnodes"] > names["stairs"])
end
