-- test ABMs with different chances

local S = core.get_translator("testnodes")

-- ABM chance 5 node
core.register_node("testabms:chance_5", {
	description = S("Node for test ABM chance_5"),
	drawtype = "normal",
	tiles = { "testabms_wait_node.png" },

	groups = { dig_immediate = 3 },
	
	on_construct = function (pos)
		local meta = core.get_meta(pos)
		meta:set_string("infotext", "Waiting for ABM testabms:chance_5")
	end,
})

core.register_abm({
	label = "testabms:chance_5",
	nodenames = "testabms:chance_5",
	interval = 10,
	chance = 5,
	action = function (pos)
		core.swap_node(pos, {name="testabms:after_abm"})
		local meta = core.get_meta(pos)
		meta:set_string("infotext", "ABM testabsm:chance_5 changed this node.")
	end
})

-- ABM chance 20 node
core.register_node("testabms:chance_20", {
	description = S("Node for test ABM chance_20"),
	drawtype = "normal",
	tiles = { "testabms_wait_node.png" },

	groups = { dig_immediate = 3 },
	
	on_construct = function (pos)
		local meta = core.get_meta(pos)
		meta:set_string("infotext", "Waiting for ABM testabms:chance_20")
	end,
})

core.register_abm({
	label = "testabms:chance_20",
	nodenames = "testabms:chance_20",
	interval = 10,
	chance = 20,
	action = function (pos)
		core.swap_node(pos, {name="testabms:after_abm"})
		local meta = core.get_meta(pos)
		meta:set_string("infotext", "ABM testabsm:chance_20 changed this node.")
	end
})

