-- test ABMs with neighbor and without_neighbor

local S = core.get_translator("testnodes")

-- ABM required neighbor
core.register_node("testabms:required_neighbor", {
	description = S("Node for test ABM required_neighbor.") .. "\n"
		.. S("Sensitive neighbor node is testnodes:normal."),
	drawtype = "normal",
	tiles = { "testabms_wait_node.png" },

	groups = { dig_immediate = 3 },
	
	on_construct = function (pos)
		local meta = core.get_meta(pos)
		meta:set_string("infotext",
			"Waiting for ABM testabms:required_neighbor "
			.. "(normal drawtype testnode sensitive)")
	end,
})

core.register_abm({
	label = "testabms:required_neighbor",
	nodenames = "testabms:required_neighbor",
	neighbors = {"testnodes:normal"},
	interval = 1,
	chance = 1,
	action = function (pos)
		core.swap_node(pos, {name="testabms:after_abm"})
		local meta = core.get_meta(pos)
		meta:set_string("infotext",
			"ABM testabsm:required_neighbor changed this node.")
	end
})

-- ABM missing neighbor node
core.register_node("testabms:missing_neighbor", {
	description = S("Node for test ABM missing_neighbor.") .. "\n"
		.. S("Sensitive neighbor node is testnodes:normal."),
	drawtype = "normal",
	tiles = { "testabms_wait_node.png" },

	groups = { dig_immediate = 3 },
	
	on_construct = function (pos)
		local meta = core.get_meta(pos)
		meta:set_string("infotext",
			"Waiting for ABM testabms:missing_neighbor"
			.. " (normal drawtype testnode sensitive)")
	end,
})

core.register_abm({
	label = "testabms:missing_neighbor",
	nodenames = "testabms:missing_neighbor",
	without_neighbors = {"testnodes:normal"},
	interval = 1,
	chance = 1,
	action = function (pos)
		core.swap_node(pos, {name="testabms:after_abm"})
		local meta = core.get_meta(pos)
		meta:set_string("infotext",
			"ABM testabsm:missing_neighbor changed this node.")
	end
})

-- ABM required and missing neighbor node
core.register_node("testabms:required_missing_neighbor", {
	description = S("Node for test ABM required_missing_neighbor.") .. "\n"
		.. S("Sensitive neighbor nodes are testnodes:normal and testnodes:glasslike."),
	drawtype = "normal",
	tiles = { "testabms_wait_node.png" },

	groups = { dig_immediate = 3 },
	
	on_construct = function (pos)
		local meta = core.get_meta(pos)
		meta:set_string("infotext",
			"Waiting for ABM testabms:required_missing_neighbor"
			.. " (wint normal drawtype testnode and no glasslike"
			.. " drawtype testnode sensitive)")
	end,
})

core.register_abm({
	label = "testabms:required_missing_neighbor",
	nodenames = "testabms:required_missing_neighbor",
	neighbors = {"testnodes:normal"},
	without_neighbors = {"testnodes:glasslike"},
	interval = 1,
	chance = 1,
	action = function (pos)
		core.swap_node(pos, {name="testabms:after_abm"})
		local meta = core.get_meta(pos)
		meta:set_string("infotext",
			"ABM testabsm:required_missing_neighbor changed this node.")
	end
})

