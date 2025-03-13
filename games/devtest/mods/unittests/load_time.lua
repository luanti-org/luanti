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
