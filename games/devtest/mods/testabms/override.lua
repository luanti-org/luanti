-- test ABMs with override

local S = core.get_translator("testabms")

-- ABM override
core.register_node("testabms:override", {
	description = S("Node for test ABM override"),
	drawtype = "normal",
	tiles = { "testabms_wait_node.png" },

	groups = { dig_immediate = 3 },
	
	on_construct = function (pos)
		local meta = core.get_meta(pos)
		meta:set_string("infotext", "Waiting for ABM testabms:overrid")
	end,
})

core.register_abm({
		label = "testabms:override",
		name = "testabms:override",
		nodenames = "testabms:override",
		interval = 1000,
		chance = 5000,
		action = function (pos)
			core.swap_node(pos, {name="testabms:after_abm"})
			local meta = core.get_meta(pos)
			meta:set_string("infotext", "ABM testabms:override changed this node.")
		end
	})

core.override_abm("testabms:override", {
		interval = 1,
		chance = 1,
		action = function (pos)
			core.swap_node(pos, {name="testabms:after_abm"})
			local meta = core.get_meta(pos)
			meta:set_string("infotext", "Override ABM testabms:override changed this node.")
		end
	})
