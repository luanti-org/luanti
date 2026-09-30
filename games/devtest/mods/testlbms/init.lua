local S = minetest.get_translator("testlbms")

-- After LBM node
minetest.register_node("testlbms:after_lbm", {
	description = S("After LBM processed node."),
	drawtype = "normal",
	tiles = { "testlbms_after_node.png" },

	groups = { dig_immediate = 3 },
})

-- LBM onload change
minetest.register_node("testlbms:onload_change", {
	description = S("Node for test LBM"),
	drawtype = "normal",
	tiles = { "testlbms_wait_node.png" },

	groups = { dig_immediate = 3 },
	
	on_construct = function (pos)
		local meta = minetest.get_meta(pos)
		meta:set_string("infotext", "Waiting for LBM testlbms:chance_5")
	end,
})

minetest.register_lbm({
		label = "testlbms:onload_change",
		name = "testlbms:onload_change",
		nodenames = "testlbms:onload_change",
		run_at_every_load = true,
		action = function (pos)
			minetest.swap_node(pos, {name="testlbms:after_lbm"})
			local meta = minetest.get_meta(pos)
			meta:set_string("infotext", "LBM testlbms:onload_change changed this node.")
		end
	})

minetest.override_lbm("testlbms:onload_change", {
		action = function (pos)
			minetest.swap_node(pos, {name="testlbms:after_lbm"})
			local meta = minetest.get_meta(pos)
			meta:set_string("infotext", "Override LBM testlbms:onload_change changed this node.")
		end,
	})

