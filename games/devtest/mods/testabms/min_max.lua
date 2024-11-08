-- test ABMs with min_y and max_y

local S = core.get_translator("testnodes")

-- ABM min_y node
core.register_node("testabms:min_y", {
	description = S("Node for test ABM min_y."),
	drawtype = "normal",
	tiles = { "testabms_wait_node.png" },

	groups = { dig_immediate = 3 },
	
	on_construct = function (pos)
		local meta = core.get_meta(pos)
		meta:set_string("infotext", "Waiting for ABM testabms:min_y at y "..pos.y.." with min_y = 0")
	end,
})

core.register_abm({
	label = "testabms:min_y",
	nodenames = "testabms:min_y",
	interval = 10,
	chance = 1,
	min_y = 0,
	action = function (pos)
		core.swap_node(pos, {name="testabms:after_abm"})
		local meta = core.get_meta(pos)
		meta:set_string("infotext", "ABM testabsm:min_y changed this node.")
	end
})

-- ABM max_y node
core.register_node("testabms:max_y", {
	description = S("Node for test ABM max_y."),
	drawtype = "normal",
	tiles = { "testabms_wait_node.png" },

	groups = { dig_immediate = 3 },
	
	on_construct = function (pos)
		local meta = core.get_meta(pos)
		meta:set_string("infotext", "Waiting for ABM testabms:max_y at y "..pos.y.." with max_y = 0")
	end,
})

core.register_abm({
	label = "testabms:max_y",
	nodenames = "testabms:max_y",
	interval = 10,
	chance = 1,
	max_y = 0,
	action = function (pos)
		core.swap_node(pos, {name="testabms:after_abm"})
		local meta = core.get_meta(pos)
		meta:set_string("infotext", "ABM testabsm:max_y changed this node.")
	end
})

