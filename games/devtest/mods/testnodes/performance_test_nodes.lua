-- Performance test mesh nodes

local S = core.get_translator("testnodes")

for use_texture_alpha, description in pairs({
	opaque = S("Marble with 'opaque' transparency"),
	clip = S("Marble with 'clip' transparency"),
	blend = S("Marble with 'blend' transparency"),
}) do
	core.register_node("testnodes:performance_mesh_" .. use_texture_alpha, {
		description = S("Performance Test Node") .. "\n" .. description,
		drawtype = "mesh",
		mesh = "testnodes_marble_glass.obj",
		tiles = {"testnodes_marble_glass.png"},
		paramtype = "light",
		use_texture_alpha = use_texture_alpha,

		groups = {dig_immediate=3},
	})
end

-- Overlay
core.register_node("testnodes:performance_overlay_clip", {
	description = S("Performance Test Node") .. "\n" .. S("Marble with overlay with 'clip' transparency") .. "\n" .. S("Palette for demonstration"),
	drawtype = "mesh",
	mesh = "testnodes_marble_metal.obj",
	tiles = {"testnodes_marble_metal.png"},
	overlay_tiles = {{name = "testnodes_marble_metal_overlay.png", color = "white"}},
	paramtype = "light",
	paramtype2 = "color",
	palette = "testnodes_palette_metal.png",
	color = "#705216";
	use_texture_alpha = "clip",

	groups = {dig_immediate=3},
})

-- Overlay
core.register_node("testnodes:performance_overlay_blend", {
	description = S("Performance Test Node") .. "\n" .. S("Marble with overlay with 'blend' transparency") .. "\n" .. S("Palette for demonstration"),
	drawtype = "mesh",
	mesh = "testnodes_marble_metal.obj",
	tiles = {"testnodes_marble_metal.png"},
	overlay_tiles = {{name = "testnodes_marble_metal_overlay.png", color = "white"}},
	paramtype = "light",
	paramtype2 = "color",
	palette = "testnodes_palette_metal.png",
	color = "#705216";
	use_texture_alpha = "blend",

	groups = {dig_immediate=3},
})
