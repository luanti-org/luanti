-- Node texture tests

local S = core.get_translator("testnodes")

core.register_node("testnodes:6sides", {
	description = S("Six Textures Test Node").."\n"..
		S("Has 1 texture per face"),
	tiles = {
		"testnodes_normal1.png",
		"testnodes_normal2.png",
		"testnodes_normal3.png",
		"testnodes_normal4.png",
		"testnodes_normal5.png",
		"testnodes_normal6.png",
	},

	groups = { dig_immediate = 2 },
})

core.register_node("testnodes:anim", {
	description = S("Animated Test Node").."\n"..
		S("Tiles animate from A to D in 4s cycle"),
	tiles = {
		{ name = "testnodes_anim.png",
		animation = {
			type = "vertical_frames",
			aspect_w = 16,
			aspect_h = 16,
			length = 4.0,
		}, },
	},

	groups = { dig_immediate = 2 },
})

core.register_node("testnodes:fill_positioning", {
	description = S("Fill Modifier Test Node") .. "\n" ..
		S("The node should have the same look as " ..
		"testnodes:fill_positioning_reference."),
	drawtype = "glasslike",
	paramtype = "light",
	tiles = {"[fill:16x16:#ffffff^[fill:6x6:1,1:#00ffdc" ..
		"^[fill:6x6:1,9:#00ffdc^[fill:6x6:9,1:#00ffdc^[fill:6x6:9,9:#00ffdc"},
	groups = {dig_immediate = 3},
})

core.register_node("testnodes:fill_positioning_reference", {
	description = S("Fill Modifier Test Node Reference"),
	drawtype = "glasslike",
	paramtype = "light",
	tiles = {"testnodes_fill_positioning_reference.png"},
	groups = {dig_immediate = 3},
})

core.register_node("testnodes:modifier_mask", {
	description = S("[mask Modifier Test Node"),
	tiles = {"testnodes_128x128_rgb.png^[mask:testnodes_mask_WRGBKW.png"},
	groups = {dig_immediate = 3},
})

-- Node texture transparency test

local alphas = { 64, 128, 191 }

for a=1,#alphas do
	local alpha = alphas[a]

	-- Transparency taken from texture
	core.register_node("testnodes:alpha_texture_"..alpha, {
		description = S("Texture Alpha Test Node (@1)", alpha).."\n"..
			S("Semi-transparent"),
		drawtype = "glasslike",
		paramtype = "light",
		tiles = {
			"testnodes_alpha"..alpha..".png",
		},
		use_texture_alpha = "blend",

		groups = { dig_immediate = 3 },
	})

	-- Transparency set via texture modifier
	core.register_node("testnodes:alpha_"..alpha, {
		description = S("Alpha Test Node (@1)", alpha).."\n"..
			S("Semi-transparent"),
		drawtype = "glasslike",
		paramtype = "light",
		tiles = {
			"testnodes_alpha.png^[opacity:" .. alpha,
		},
		use_texture_alpha = "blend",

		groups = { dig_immediate = 3 },
	})
end

core.register_node("testnodes:alpha_compositing", {
	description = S("Texture Overlay Test Node") .. "\n" ..
		S("A regular grid should be visible where each cell contains two " ..
		"texels with the same color.") .. "\n" ..
		S("Texture overlay is gamma-incorrect, " ..
		"and in general it does not do alpha compositing, " ..
		"both for backwards compatibility."),
	drawtype = "glasslike",
	paramtype = "light",
	tiles = {"testnodes_alpha_compositing_bottom.png^"  ..
		"testnodes_alpha_compositing_top.png"},
	use_texture_alpha = "blend",
	groups = {dig_immediate = 3},
})

-- Generate PNG textures

local function mandelbrot(w, h, iterations)
	local r = {}
	for y=0, h-1 do
		for x=0, w-1 do
			local re = (x - w/2) * 4/w
			local im = (y - h/2) * 4/h
			-- zoom in on a nice view
			re = re / 128 - 0.23
			im = im / 128 - 0.82

			local px, py = 0, 0
			local i = 0
			while px*px + py*py <= 4 and i < iterations do
				px, py = px*px - py*py + re, 2 * px * py + im
				i = i + 1
			end
			r[w*y+x+1] = i / iterations
		end
	end
	return r
end

local function gen_checkers(w, h, tile)
	local r = {}
	for y=0, h-1 do
		for x=0, w-1 do
			local hori = math.floor(x / tile) % 2 == 0
			local vert = math.floor(y / tile) % 2 == 0
			r[w*y+x+1] = hori ~= vert and 1 or 0
		end
	end
	return r
end

-- The engine should perform color reduction of the generated PNG in certain
-- cases, so we have this helper to check the result
local function encode_and_check(w, h, ctype, data)
	local ret = core.encode_png(w, h, data)
	assert(ret:sub(1, 8) == "\137PNG\r\n\026\n", "missing png signature")
	assert(ret:sub(9, 16) == "\000\000\000\rIHDR", "didn't find ihdr chunk")
	local ctype_actual = ret:byte(26) -- Color Type (1 byte)
	ctype = ({rgba=6, rgb=2, gray=0})[ctype]
	assert(ctype_actual == ctype, "png should have color type " .. ctype ..
		" but actually has " .. ctype_actual)
	return ret
end

local fractal = mandelbrot(512, 512, 128)
local frac_emb = mandelbrot(64, 64, 64)
local checker = gen_checkers(512, 512, 32)

local floor = math.floor
local abs = math.abs
local data_emb = {}
local data_mb = {}
local data_ck = {}
for i=1, #frac_emb do
	data_emb[i] = {
		r = floor(abs(frac_emb[i] * 2 - 1) * 255),
		g = floor(abs(1 - frac_emb[i]) * 255),
		b = floor(frac_emb[i] * 255),
		a = frac_emb[i] < 0.95 and 255 or 0,
	}
end
for i=1, #fractal do
	data_mb[i] = {
		r = floor(fractal[i] * 255),
		g = floor(abs(fractal[i] * 2 - 1) * 255),
		b = floor(abs(1 - fractal[i]) * 255),
		a = 255,
	}
	data_ck[i] = checker[i] > 0 and "#888" or "#000"
end

fractal = nil
frac_emb = nil
checker = nil

local textures_path = core.get_worldpath() .. "/"
core.safe_file_write(
	textures_path .. "testnodes1.png",
	encode_and_check(512, 512, "rgb", data_mb)
)
local png_ck = encode_and_check(512, 512, "gray", data_ck)
core.dynamic_add_media({
	filename = "testnodes_generated_mb.png",
	filepath = textures_path .. "testnodes1.png"
})
core.dynamic_add_media({
	filename = "testnodes_generated_ck.png",
	filedata = png_ck,
})

core.register_node("testnodes:generated_png_mb", {
	description = S("Generated Mandelbrot PNG Test Node"),
	tiles = { "testnodes_generated_mb.png" },

	groups = { dig_immediate = 2 },
})
core.register_node("testnodes:generated_png_ck", {
	description = S("Generated Checker PNG Test Node"),
	tiles = { "testnodes_generated_ck.png" },

	groups = { dig_immediate = 2 },
})

local png_emb = "[png:" .. core.encode_base64(
	encode_and_check(64, 64, "rgba", data_emb))

core.register_node("testnodes:generated_png_emb", {
	description = S("Generated In-Band Mandelbrot PNG Test Node"),
	tiles = { png_emb },

	drawtype = "allfaces", -- required because of transparent pixels
	use_texture_alpha = "clip",
	paramtype = "light",
	groups = { dig_immediate = 2 },
})
core.register_node("testnodes:generated_png_src_emb", {
	description = S("Generated In-Band Source Blit Mandelbrot PNG Test Node"),
	tiles = { png_emb .. "^testnodes_damage_neg.png" },

	drawtype = "allfaces", -- required because of transparent pixels
	use_texture_alpha = "clip",
	paramtype = "light",
	groups = { dig_immediate = 2 },
})
core.register_node("testnodes:generated_png_dst_emb", {
	description = S("Generated In-Band Dest Blit Mandelbrot PNG Test Node"),
	tiles = { "testnodes_generated_ck.png^" .. png_emb },

	groups = { dig_immediate = 2 },
})

png_ck = nil
png_emb = nil
data_emb = nil
data_mb = nil
data_ck = nil

--[[

The following nodes can be used to demonstrate the TGA format support.

Luanti supports TGA types 1, 2, 3 & 10. While adding the support for
TGA type 9 (RLE-compressed, color-mapped) is easy, it is not advisable
to do so, as it is not backwards compatible with any engine version pre-5.5;
content creators should therefore either use TGA type 1 or 10, or PNG.

TODO: Types 1, 2 & 10 should have two test nodes each (i.e. bottom-top
and top-bottom) for 16bpp (A1R5G5B5), 24bpp (B8G8R8), 32bpp (B8G8R8A8)
colors.

Note: Luanti requires the optional TGA footer for a texture to load.
If a TGA image does not load in Luanti, append eight (8) null bytes,
then the string “TRUEVISION-XFILE.”, then another null byte.

]]--

core.register_node("testnodes:tga_type1_24bpp_bt", {
	description = S("TGA Type 1 (color-mapped RGB) 24bpp bottom-top Test Node"),
	drawtype = "glasslike",
	paramtype = "light",
	sunlight_propagates = true,
	tiles = { "testnodes_tga_type1_24bpp_bt.tga" },
	groups = { dig_immediate = 2 },
})

core.register_node("testnodes:tga_type1_24bpp_tb", {
	description = S("TGA Type 1 (color-mapped RGB) 24bpp top-bottom Test Node"),
	drawtype = "glasslike",
	paramtype = "light",
	sunlight_propagates = true,
	tiles = { "testnodes_tga_type1_24bpp_tb.tga" },
	groups = { dig_immediate = 2 },
})

core.register_node("testnodes:tga_type2_16bpp_bt", {
	description = S("TGA Type 2 (uncompressed RGB) 16bpp bottom-top Test Node"),
	drawtype = "glasslike",
	paramtype = "light",
	sunlight_propagates = true,
	tiles = { "testnodes_tga_type2_16bpp_bt.tga" },
	use_texture_alpha = "clip",
	groups = { dig_immediate = 2 },
})

core.register_node("testnodes:tga_type2_16bpp_tb", {
	description = S("TGA Type 2 (uncompressed RGB) 16bpp top-bottom Test Node"),
	drawtype = "glasslike",
	paramtype = "light",
	sunlight_propagates = true,
	tiles = { "testnodes_tga_type2_16bpp_tb.tga" },
	use_texture_alpha = "clip",
	groups = { dig_immediate = 2 },
})

core.register_node("testnodes:tga_type2_32bpp_bt", {
	description = S("TGA Type 2 (uncompressed RGB) 32bpp bottom-top Test Node"),
	drawtype = "glasslike",
	paramtype = "light",
	sunlight_propagates = true,
	tiles = { "testnodes_tga_type2_32bpp_bt.tga" },
	use_texture_alpha = "blend",
	groups = { dig_immediate = 2 },
})

core.register_node("testnodes:tga_type2_32bpp_tb", {
	description = S("TGA Type 2 (uncompressed RGB) 32bpp top-bottom Test Node"),
	drawtype = "glasslike",
	paramtype = "light",
	sunlight_propagates = true,
	tiles = { "testnodes_tga_type2_32bpp_tb.tga" },
	use_texture_alpha = "blend",
	groups = { dig_immediate = 2 },
})

core.register_node("testnodes:tga_type3_16bpp_bt", {
	description = S("TGA Type 3 (uncompressed grayscale) 16bpp bottom-top Test Node"),
	drawtype = "glasslike",
	paramtype = "light",
	sunlight_propagates = true,
	tiles = { "testnodes_tga_type3_16bpp_bt.tga" },
	use_texture_alpha = "blend",
	groups = { dig_immediate = 2 },
})

core.register_node("testnodes:tga_type3_16bpp_tb", {
	description = S("TGA Type 3 (uncompressed grayscale) 16bpp top-bottom Test Node"),
	drawtype = "glasslike",
	paramtype = "light",
	sunlight_propagates = true,
	tiles = { "testnodes_tga_type3_16bpp_tb.tga" },
	use_texture_alpha = "blend",
	groups = { dig_immediate = 2 },
})

core.register_node("testnodes:tga_type10_32bpp_bt", {
	description = S("TGA Type 10 (RLE-compressed RGB) 32bpp bottom-top Test Node"),
	tiles = { "testnodes_tga_type10_32bpp_bt.tga" },
	drawtype = "glasslike",
	paramtype = "light",
	sunlight_propagates = true,
	use_texture_alpha = "blend",
	groups = { dig_immediate = 2 },
})

core.register_node("testnodes:tga_type10_32bpp_tb", {
	description = S("TGA Type 10 (RLE-compressed RGB) 32bpp top-bottom Test Node"),
	drawtype = "glasslike",
	paramtype = "light",
	sunlight_propagates = true,
	tiles = { "testnodes_tga_type10_32bpp_tb.tga" },
	use_texture_alpha = "blend",
	groups = { dig_immediate = 2 },
})
