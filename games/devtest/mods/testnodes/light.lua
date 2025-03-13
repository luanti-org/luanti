-- Test Nodes: Light test

local S = core.get_translator("testnodes")

-- All possible light levels
for i=1, core.LIGHT_MAX do
	core.register_node("testnodes:light"..string.format("%02d", i), {
		description = S("Light Source (@1)", i),
		paramtype = "light",
		light_source = i,


		tiles ={"testnodes_light_"..i..".png"},
		drawtype = "glasslike",
		walkable = false,
		sunlight_propagates = true,
		is_ground_content = false,
		groups = {dig_immediate=3},
	})
end

-- Lets light through, but not sunlight, leading to a
-- reduction in light level when light passes through
core.register_node("testnodes:sunlight_filter", {
	description = S("Sunlight Filter") .."\n"..
		S("Lets light through, but weakens sunlight"),
	paramtype = "light",


	drawtype = "glasslike",
	tiles = {
		"testnodes_sunlight_filter.png",
	},
	groups = { dig_immediate = 3 },
})

-- Lets light and sunlight through without obstruction
core.register_node("testnodes:sunlight_propagator", {
	description = S("Sunlight Propagator") .."\n"..
		S("Lets all light through"),
	paramtype = "light",
	sunlight_propagates = true,


	drawtype = "glasslike",
	tiles = {
		"testnodes_sunlight_filter.png^[brighten",
	},
	groups = { dig_immediate = 3 },
})
