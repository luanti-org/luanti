local S = core.get_translator("testnodes")

core.register_node("testnodes:overlay", {
	description = S("Texture Overlay Test Node") .. "\n" ..
		S("Uncolorized"),
	tiles = {{name = "testnodes_overlayable.png"}},
	overlay_tiles = {{name = "testnodes_overlay.png"}},
	groups = { dig_immediate = 2 },
})
core.register_node("testnodes:overlay_color_all", {
	description = S("Texture Overlay Test Node, Colorized") .. "\n" ..
		S("param2 changes color"),
	tiles = {{name = "testnodes_overlayable.png"}},
	overlay_tiles = {{name = "testnodes_overlay.png"}},
	paramtype2 = "color",
	palette = "testnodes_palette_full.png",


	groups = { dig_immediate = 2 },
})
core.register_node("testnodes:overlay_color_overlay", {
	description = S("Texture Overlay Test Node, Colorized Overlay") .. "\n" ..
		S("param2 changes color of overlay"),
	tiles = {{name = "testnodes_overlayable.png", color="white"}},
	overlay_tiles = {{name = "testnodes_overlay.png"}},
	paramtype2 = "color",
	palette = "testnodes_palette_full.png",


	groups = { dig_immediate = 2 },
})
core.register_node("testnodes:overlay_color_overlayed", {
	description = S("Texture Overlay Test Node, Colorized Base") .. "\n" ..
		S("param2 changes color of base texture"),
	tiles = {{name = "testnodes_overlayable.png"}},
	overlay_tiles = {{name = "testnodes_overlay.png", color="white"}},
	paramtype2 = "color",
	palette = "testnodes_palette_full.png",


	groups = { dig_immediate = 2 },
})

local global_overlay_color = "#FF2000"
core.register_node("testnodes:overlay_global", {
	description = S("Texture Overlay Test Node, Global Color") .. "\n" ..
		S("Global color = @1", global_overlay_color),
	tiles = {{name = "testnodes_overlayable.png"}},
	overlay_tiles = {{name = "testnodes_overlay.png"}},
	color = global_overlay_color,


	groups = { dig_immediate = 2 },
})
core.register_node("testnodes:overlay_global_color_all", {
	description = S("Texture Overlay Test Node, Global Color + Colorized") .. "\n" ..
		S("Global color = @1", global_overlay_color) .. "\n" ..
		S("param2 changes color"),
	tiles = {{name = "testnodes_overlayable.png"}},
	overlay_tiles = {{name = "testnodes_overlay.png"}},
	color = global_overlay_color,
	paramtype2 = "color",
	palette = "testnodes_palette_full.png",


	groups = { dig_immediate = 2 },
})
core.register_node("testnodes:overlay_global_color_overlay", {
	description = S("Texture Overlay Test Node, Global Color + Colorized Overlay") .. "\n" ..
		S("Global color = @1", global_overlay_color) .. "\n" ..
		S("param2 changes color of overlay"),
	tiles = {{name = "testnodes_overlayable.png", color=global_overlay_color}},
	overlay_tiles = {{name = "testnodes_overlay.png"}},
	color = global_overlay_color,
	paramtype2 = "color",
	palette = "testnodes_palette_full.png",


	groups = { dig_immediate = 2 },
})
core.register_node("testnodes:overlay_global_color_overlayed", {
	description = S("Texture Overlay Test Node, Global Color + Colorized Base") .. "\n" ..
		S("Global color = @1", global_overlay_color) .. "\n" ..
		S("param2 changes color of base texture"),
	tiles = {{name = "testnodes_overlayable.png"}},
	overlay_tiles = {{name = "testnodes_overlay.png", color=global_overlay_color}},
	color = global_overlay_color,
	paramtype2 = "color",
	palette = "testnodes_palette_full.png",


	groups = { dig_immediate = 2 },
})
