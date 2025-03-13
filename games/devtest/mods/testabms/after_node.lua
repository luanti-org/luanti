
local S = core.get_translator("testnodes")

-- After ABM node
core.register_node("testabms:after_abm", {
	description = S("After ABM processed node."),
	drawtype = "normal",
	tiles = { "testabms_after_node.png" },

	groups = { dig_immediate = 3 },
})

