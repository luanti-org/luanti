-- This file is for misc. param2 tests that aren't covered in drawtypes.lua already.

local S = core.get_translator("testnodes")

core.register_node("testnodes:facedir", {
	description = S("Facedir Test Node").."\n"..
		S("param2 = facedir rotation (0..23)"),
	paramtype2 = "facedir",
	tiles = {
		"testnodes_1.png",
		"testnodes_2.png",
		"testnodes_3.png",
		"testnodes_4.png",
		"testnodes_5.png",
		"testnodes_6.png",
	},

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:4dir", {
	description = S("4dir Test Node").."\n"..
		S("param2 = 4dir rotation (0..3)"),
	paramtype2 = "4dir",
	tiles = {
		"testnodes_1f.png",
		"testnodes_2f.png",
		"testnodes_3f.png",
		"testnodes_4f.png",
		"testnodes_5f.png",
		"testnodes_6f.png",
	},

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:facedir_nodebox", {
	description = S("Facedir Nodebox Test Node").."\n"..
		S("param2 = facedir rotation (0..23)"),
	tiles = {
		"testnodes_1.png",
		"testnodes_2.png",
		"testnodes_3.png",
		"testnodes_4.png",
		"testnodes_5.png",
		"testnodes_6.png",
	},
	drawtype = "nodebox",
	paramtype = "light",
	paramtype2 = "facedir",
	node_box = {
		type = "fixed",
		fixed = {-0.5, -0.5, -0.5, 0.2, 0.2, 0.2},
	},

	groups = {dig_immediate=3},
})

core.register_node("testnodes:4dir_nodebox", {
	description = S("4dir Nodebox Test Node").."\n"..
		S("param2 = 4dir rotation (0..3)"),
	tiles = {
		"testnodes_1f.png",
		"testnodes_2f.png",
		"testnodes_3f.png",
		"testnodes_4f.png",
		"testnodes_5f.png",
		"testnodes_6f.png",
	},
	drawtype = "nodebox",
	paramtype = "light",
	paramtype2 = "4dir",
	node_box = {
		type = "fixed",
		fixed = {-0.5, -0.5, -0.5, 0.2, 0.2, 0.2},
	},

	groups = {dig_immediate=3},
})

core.register_node("testnodes:4dir_nodebox_stair", {
	description = S("4dir Nodebox Stair Test Node").."\n"..
		S("param2 = 4dir rotation (0..3)"),
	tiles = {
		"testnodes_1f.png",
		"testnodes_2f.png",
		"testnodes_3f.png",
		"testnodes_4f.png",
		"testnodes_5f.png",
		"testnodes_6f.png",
	},
	drawtype = "nodebox",
	paramtype = "light",
	paramtype2 = "4dir",
	node_box = {
		type = "fixed",
		fixed = {
			{-0.5, -0.5, -0.5, 0.5, 0, 0.5},
			{-0.5, 0, 0, 0.5, 0.5, 0.5},
		},
	},

	groups = { dig_immediate = 3 },
})


core.register_node("testnodes:wallmounted", {
	description = S("Wallmounted Test Node").."\n"..
		S("param2 = wallmounted rotation (0..7)"),
	paramtype2 = "wallmounted",
	tiles = {
		"testnodes_1w.png",
		"testnodes_2w.png",
		"testnodes_3w.png",
		"testnodes_4w.png",
		"testnodes_5w.png",
		"testnodes_6w.png",
	},

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:wallmounted_rot", {
	description = S("Wallmounted Rotatable Test Node"),
	paramtype2 = "wallmounted",
	wallmounted_rotate_vertical = true,
	tiles = {
		"testnodes_1w.png^[colorize:#FFFF00:40",
		"testnodes_2w.png^[colorize:#FFFF00:40",
		"testnodes_3w.png^[colorize:#FFFF00:40",
		"testnodes_4w.png^[colorize:#FFFF00:40",
		"testnodes_5w.png^[colorize:#FFFF00:40",
		"testnodes_6w.png^[colorize:#FFFF00:40",
	},

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:wallmounted_nodebox", {
	description = S("Wallmounted Nodebox Test Node").."\n"..
		S("param2 = wallmounted rotation (0..7)"),
	paramtype2 = "wallmounted",
	paramtype = "light",
	tiles = {
		"testnodes_1w.png",
		"testnodes_2w.png",
		"testnodes_3w.png",
		"testnodes_4w.png",
		"testnodes_5w.png",
		"testnodes_6w.png",
	},
	drawtype = "nodebox",
	node_box = {
		type = "wallmounted",
		wall_top = { -0.5, 0, -0.5, 0.5, 0.5, 0.5 },
		wall_bottom = { -0.5, -0.5, -0.5, 0.5, 0, 0.5 },
		wall_side = { -0.5, -0.5, -0.5, 0, 0.5, 0.5 },
	},

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:wallmounted_nodebox_rot", {
	description = S("Wallmounted Rotatable Nodebox Test Node"),
	paramtype2 = "wallmounted",
	wallmounted_rotate_vertical = true,
	paramtype = "light",
	tiles = {
		"testnodes_1w.png^[colorize:#FFFF00:40",
		"testnodes_2w.png^[colorize:#FFFF00:40",
		"testnodes_3w.png^[colorize:#FFFF00:40",
		"testnodes_4w.png^[colorize:#FFFF00:40",
		"testnodes_5w.png^[colorize:#FFFF00:40",
		"testnodes_6w.png^[colorize:#FFFF00:40",
	},
	drawtype = "nodebox",
	node_box = {
		type = "wallmounted",
		wall_top = { -0.5, 0, -0.5, 0.5, 0.5, 0.5 },
		wall_bottom = { -0.5, -0.5, -0.5, 0.5, 0, 0.5 },
		wall_side = { -0.5, -0.5, -0.5, 0, 0.5, 0.5 },
	},

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:color", {
	description = S("Color Test Node").."\n"..
		S("param2 = color (0..255)"),
	paramtype2 = "color",
	palette = "testnodes_palette_full.png",
	tiles = {
		"testnodes_node.png",
	},

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:colorfacedir", {
	description = S("Color Facedir Test Node").."\n"..
		S("param2 = color + facedir rotation (0..23, 32..55, ...)"),
	paramtype2 = "colorfacedir",
	palette = "testnodes_palette_facedir.png",
	tiles = {
		"testnodes_1g.png",
		"testnodes_2g.png",
		"testnodes_3g.png",
		"testnodes_4g.png",
		"testnodes_5g.png",
		"testnodes_6g.png",
	},

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:colorfacedir_nodebox", {
	description = S("Color Facedir Nodebox Test Node").."\n"..
		S("param2 = color + facedir rotation (0..23, 32..55, ...)"),
	tiles = {
		"testnodes_1g.png",
		"testnodes_2g.png",
		"testnodes_3g.png",
		"testnodes_4g.png",
		"testnodes_5g.png",
		"testnodes_6g.png",
	},
	drawtype = "nodebox",
	paramtype = "light",
	paramtype2 = "colorfacedir",
	palette = "testnodes_palette_facedir.png",
	node_box = {
		type = "fixed",
		fixed = {-0.5, -0.5, -0.5, 0.2, 0.2, 0.2},
	},

	groups = {dig_immediate=3},
})

core.register_node("testnodes:color4dir", {
	description = S("Color 4dir Test Node").."\n"..
		S("param2 = color + 4dir rotation (0..255)"),
	paramtype2 = "color4dir",
	palette = "testnodes_palette_4dir.png",
	tiles = {
		"testnodes_1fg.png",
		"testnodes_2fg.png",
		"testnodes_3fg.png",
		"testnodes_4fg.png",
		"testnodes_5fg.png",
		"testnodes_6fg.png",
	},

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:color4dir_nodebox", {
	description = S("Color 4dir Nodebox Test Node").."\n"..
		S("param2 = color + 4dir rotation (0..255)"),
	tiles = {
		"testnodes_1fg.png",
		"testnodes_2fg.png",
		"testnodes_3fg.png",
		"testnodes_4fg.png",
		"testnodes_5fg.png",
		"testnodes_6fg.png",
	},
	drawtype = "nodebox",
	paramtype = "light",
	paramtype2 = "color4dir",
	palette = "testnodes_palette_4dir.png",
	node_box = {
		type = "fixed",
		fixed = {-0.5, -0.5, -0.5, 0.2, 0.2, 0.2},
	},

	groups = {dig_immediate=3},
})

core.register_node("testnodes:colorwallmounted", {
	description = S("Color Wallmounted Test Node").."\n"..
		S("param2 = color + wallmounted rotation (0..7, 8..15, ...)"),
	paramtype2 = "colorwallmounted",
	paramtype = "light",
	palette = "testnodes_palette_wallmounted.png",
	tiles = {
		"testnodes_1wg.png",
		"testnodes_2wg.png",
		"testnodes_3wg.png",
		"testnodes_4wg.png",
		"testnodes_5wg.png",
		"testnodes_6wg.png",
	},

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:colorwallmounted_nodebox", {
	description = S("Color Wallmounted Nodebox Test Node").."\n"..
		S("param2 = color + wallmounted rotation (0..7, 8..15, ...)"),
	paramtype2 = "colorwallmounted",
	paramtype = "light",
	palette = "testnodes_palette_wallmounted.png",
	tiles = {
		"testnodes_1wg.png",
		"testnodes_2wg.png",
		"testnodes_3wg.png",
		"testnodes_4wg.png",
		"testnodes_5wg.png",
		"testnodes_6wg.png",
	},
	drawtype = "nodebox",
	node_box = {
		type = "wallmounted",
		wall_top = { -0.5, 0, -0.5, 0.5, 0.5, 0.5 },
		wall_bottom = { -0.5, -0.5, -0.5, 0.5, 0, 0.5 },
		wall_side = { -0.5, -0.5, -0.5, 0, 0.5, 0.5 },
	},

	groups = { dig_immediate = 3 },
})

