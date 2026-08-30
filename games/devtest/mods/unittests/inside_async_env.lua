unittests = {}

core.log("info", "Hello World")

unittests.custom_metatable = {}
core.register_portable_metatable("unittests:custom_metatable", unittests.custom_metatable)

local function do_tests()
	assert(core == minetest)
	-- stuff that should not be here
	assert(not core.get_player_by_name)
	assert(not core.set_node)
	assert(not core.object_refs)
	-- stuff that should be here
	assert(require("unittests.require.foo") == "foo")
	assert(ItemStack)
	local meta = ItemStack():get_meta()
	assert(type(meta) == "userdata")
	assert(type(meta.set_tool_capabilities) == "function")
	assert(core.registered_items[""])
	assert(next(core.registered_nodes) ~= nil)
	assert(core.registered_craftitems["unittests:stick"])
	-- alias handling
	assert(core.registered_items["unittests:steel_ingot_alias"].name ==
		"unittests:steel_ingot")
	-- fallback to item defaults
	assert(core.registered_items["unittests:description_test"].on_place == true)
end

function unittests.async_test()
	local ok, err = pcall(do_tests)
	if not ok then
		core.log("error", err)
	end
	return ok
end
