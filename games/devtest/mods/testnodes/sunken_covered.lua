local S = minetest.get_translator("testnodes")

-- Sunken node example.
minetest.register_node("testnodes:sunken_ls_r5", {
	description = "Sunken Test Node in Liquid Source Range 5",
	drawtype = "sunken",
	tiles = {"testnodes_liquidsource_r5.png"},
	special_tiles = {
		{name = "testnodes_liquidsource_r5.png", backface_culling = false},
		{name = "testnodes_liquidsource_r5.png", backface_culling = true},
	},
	use_texture_alpha = "blend",
	paramtype = "light",
	walkable = false,
	buildable_to = true,
	is_ground_content = false,
	liquidtype = "source",
	liquid_alternative_flowing = "testnodes:rliquid_flowing_5",
	liquid_alternative_source = "testnodes:rliquid_5",
	--liquid_alternative_flowing = "testnodes:sunken_nodebox",
	--liquid_alternative_source = "testnodes:sunken_nodebox",
	liquid_range = 5,

	on_construct = function(pos)
		local meta = minetest.get_meta(pos)
		meta:set_string("inner_node", "testnodes:mesh")
	end,
})


-- Covered node example.
minetest.register_node("testnodes:covered_nodebox", {
	description = S("Covered Test Node in Nodebox Test Node").."\n"..
		S("Torchlike node inside"),
	tiles = { "testnodes_nodebox.png" },
	drawtype = "covered",
	paramtype2 = "leveled",
	paramtype = "light",

	node_box = {
		type = "leveled",
		fixed = {-0.5, 0.0, -0.5, 0.5, -0.499, 0.5},
	},

	groups = {dig_immediate=3},

	on_construct = function(pos)
		local meta = minetest.get_meta(pos)
		meta:set_string("inner_node", "testnodes:mesh")
	end,
})

