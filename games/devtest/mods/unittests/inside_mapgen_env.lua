core.log("info", "Hello World")

local function do_tests()
	assert(core == minetest)
	-- stuff that should not be here
	assert(not core.get_player_by_name)
	assert(not core.object_refs)
	-- stuff that should be here
	assert(core.register_on_generated)
	assert(core.get_node)
	assert(core.spawn_tree)
	assert(ItemStack)
	local meta = ItemStack():get_meta()
	assert(type(meta) == "userdata")
	assert(type(meta.set_tool_capabilities) == "function")
	assert(core.registered_items[""])
	assert(core.save_gen_notify)
	-- alias handling
	assert(core.registered_items["unittests:steel_ingot_alias"].name ==
		"unittests:steel_ingot")
	-- fallback to item defaults
	assert(core.registered_items["unittests:description_test"].on_place == true)
end

-- first thread to get here runs the tests
if core.ipc_cas("unittests:mg_once", nil, true) then
	-- this is checked from the main env
	core.ipc_set("unittests:mg", { pcall(do_tests) })
end

core.register_on_generated(function(vm, pos1, pos2, blockseed)
	local cs = core.get_mapgen_chunksize()
	assert(pos2:subtract(pos1) == cs:multiply(core.MAP_BLOCKSIZE):subtract(1))
end)
