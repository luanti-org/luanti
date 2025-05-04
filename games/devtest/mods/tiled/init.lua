local align_help = "Texture spans over a space of 8×8 nodes"
local align_help_n = "Tiles looks the same for every node"

local scaled_tile = {
	name = "tiled_tiled.png",
	align_style = "world",
	scale = 8,
}

core.register_node("tiled:tiled", {
	description = "Tiled 'normal' Node (world-aligned)".."\n"..align_help,
	tiles = {scaled_tile},
	groups = {cracky=3},
})

core.register_node("tiled:tiled_n", {
	description = "Tiled 'normal' Node (node-aligned)".."\n"..align_help_n,
	tiles = {{name="tiled_tiled_node.png", align_style="node"}},
	groups = {cracky=3},
})

stairs.register_stair_and_slab("tiled_n", "tiled:tiled_n",
		{cracky=3},
		{{name="tiled_tiled_node.png", align_style="node"}},
		"Tiled Stair (node-aligned)".."\n"..align_help_n,
		"Tiled Slab (node-aligned)".."\n"..align_help_n)

stairs.register_stair_and_slab("tiled", "tiled:tiled",
		{cracky=3},
		{scaled_tile},
		"Tiled Stair (world-aligned)".."\n"..align_help,
		"Tiled Slab (world-aligned)".."\n"..align_help)

core.register_node("tiled:tiled_liquid", {
	description =
		"Tiled 'liquid' Node (world-aligned)".."\n"..
		align_help.."\n"..
		"(waving = 3)",
	paramtype = "light",
	drawtype = "liquid",
	tiles = {scaled_tile},
	groups = {cracky=3},
	waving = 3,
	liquidtype = "none",
	buildable_to = true,
})

core.register_node("tiled:tiled_glasslike", { -- broken
	description =
		"Tiled 'glasslike' Node (world-aligned)".."\n"..
		align_help,
	paramtype = "light",
	paramtype2 = "facedir",
	drawtype = "glasslike",
	tiles = {scaled_tile},
	groups = {cracky=3},
})

core.register_node("tiled:tiled_glasslike_framed", {
	description =
		"Tiled 'glasslike_framed' Node (world-aligned)".."\n"..
		align_help,
	paramtype = "light",
	drawtype = "glasslike_framed",
	tiles = {scaled_tile, scaled_tile},
	groups = {cracky=3},
})

core.register_node("tiled:tiled_glasslike_framed", {
	description =
		"Tiled 'glasslike_framed' Node (world-aligned)".."\n"..
		align_help,
	paramtype = "light",
	drawtype = "glasslike_framed",
	tiles = {scaled_tile, "testnodes_glasslike_detail.png"},
	groups = {cracky=3},
})

core.register_node("tiled:tiled_glasslike_framed_optional", {
	description =
		"Tiled 'glasslike_framed_optional' Node (world-aligned)".."\n"..
		align_help,
	paramtype = "light",
	drawtype = "glasslike_framed_optional",
	tiles = {scaled_tile, "testnodes_glasslike_detail.png"},
	groups = {cracky=3},
})

core.register_node("tiled:tiled_rooted", {
	description =
		"Tiled 'plantlike_rooted' Node (world-aligned)".."\n"..
		"Base node texture spans over a space of 8×8 nodes".."\n"..
		"A plantlike thing grows on top",
	paramtype = "light",
	drawtype = "plantlike_rooted",
	tiles = {scaled_tile},
	special_tiles = {"tiled_tiled_node.png"},
	groups = {cracky=3},
})
