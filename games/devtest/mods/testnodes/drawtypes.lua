--[[ Drawtype Test: This file tests out and provides examples for
all drawtypes in Luanti. It is attempted to keep the node
definitions as simple and minimal as possible to keep
side-effects to a minimum.

How to read the node definitions:
There are two parts which are separated by 2 newlines:
The first part contains the things that are more or less essential
for defining the drawtype (except description, which is
at the top for readability).
The second part (after the 2 newlines) contains stuff that are
unrelated to the drawtype, stuff that is mostly there to make
testing this node easier and more convenient.
]]

local S = core.get_translator("testnodes")

-- A regular cube
core.register_node("testnodes:normal", {
	description = S("\"normal\" Drawtype Test Node").."\n"..
		S("Opaque texture"),
	drawtype = "normal",
	tiles = { "testnodes_normal.png" },

	groups = { dig_immediate = 3 },
})

-- Standard glasslike node
core.register_node("testnodes:glasslike", {
	description = S("\"glasslike\" Drawtype Test Node").."\n"..
		S("Transparent node with hidden backfaces"),
	drawtype = "glasslike",
	paramtype = "light",
	tiles = { "testnodes_glasslike.png" },

	groups = { dig_immediate = 3 },
})

-- Glasslike framed with the two textures (normal and "detail")
core.register_node("testnodes:glasslike_framed", {
	description = S("\"glasslike_framed\" Drawtype Test Node").."\n"..
		S("Transparent node with hidden backfaces").."\n"..
		S("Frame connects to neighbors"),
	drawtype = "glasslike_framed",
	paramtype = "light",
	tiles = {
		"testnodes_glasslike_framed.png",
		"testnodes_glasslike_detail.png",
	},


	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})

-- Like the one above, but without the "detail" texture (texture 2).
-- This node was added to see how the engine behaves when the "detail" texture
-- is missing.
core.register_node("testnodes:glasslike_framed_no_detail", {
	description = S("\"glasslike_framed\" Drawtype without Detail Test Node").."\n"..
		S("Transparent node with hidden backfaces").."\n"..
		S("Frame connects to neighbors, but the 'detail' tile is not used"),
	drawtype = "glasslike_framed",
	paramtype = "light",
	tiles = { "testnodes_glasslike_framed2.png" },


	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})


core.register_node("testnodes:glasslike_framed_optional", {
	description = S("\"glasslike_framed_optional\" Drawtype Test Node").."\n"..
		S("Transparent node with hidden backfaces").."\n"..
		S("Frame connects if 'connected_glass' setting is true"),
	drawtype = "glasslike_framed_optional",
	paramtype = "light",
	tiles = {
		"testnodes_glasslike_framed_optional.png",
		"testnodes_glasslike_detail.png",
	},


	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})



core.register_node("testnodes:allfaces", {
	description = S("\"allfaces\" Drawtype Test Node").."\n"..
		S("Transparent node with visible internal backfaces"),
	drawtype = "allfaces",
	paramtype = "light",
	tiles = { "testnodes_allfaces.png" },

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:allfaces_6", {
	description = S("\"allfaces 6 Textures\" Drawtype Test Node").."\n"..
		S("Transparent node with visible internal backfaces"),
	drawtype = "allfaces",
	paramtype = "light",
	tiles = {
			"testnodes_allfaces.png^[colorize:red",
			"testnodes_allfaces.png^[colorize:orange",
			"testnodes_allfaces.png^[colorize:yellow",
			"testnodes_allfaces.png^[colorize:green",
			"testnodes_allfaces.png^[colorize:blue",
			"testnodes_allfaces.png^[colorize:purple"
		},

	groups = { dig_immediate = 3 },
})

local allfaces_optional_tooltip = ""..
	S("Rendering depends on 'leaves_style' setting:").."\n"..
	S("* 'fancy': transparent with visible internal backfaces").."\n"..
	S("* 'simple': transparent with hidden backfaces").."\n"..
	S("* 'opaque': opaque")

core.register_node("testnodes:allfaces_optional", {
	description = S("\"allfaces_optional\" Drawtype Test Node").."\n"..
		allfaces_optional_tooltip,
	drawtype = "allfaces_optional",
	paramtype = "light",
	tiles = { "testnodes_allfaces_optional.png" },

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:allfaces_optional_waving", {
	description = S("Waving \"allfaces_optional\" Drawtype Test Node").."\n"..
		allfaces_optional_tooltip.."\n"..
		S("Waves if waving leaves are enabled by client"),
	drawtype = "allfaces_optional",
	paramtype = "light",
	tiles = { "testnodes_allfaces_optional.png^[brighten" },
	waving = 2,

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:firelike", {
	description = S("\"firelike\" Drawtype Test Node").."\n"..
		S("Changes shape based on neighbors"),
	drawtype = "firelike",
	paramtype = "light",
	tiles = { "testnodes_firelike.png" },


	walkable = false,
	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:fencelike", {
	description = S("\"fencelike\" Drawtype Test Node").."\n"..
		S("Connects to neighbors"),
	drawtype = "fencelike",
	paramtype = "light",
	tiles = { "testnodes_fencelike.png" },

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:torchlike", {
	description = S("Floor \"torchlike\" Drawtype Test Node").."\n"..
		S("Always on floor"),
	drawtype = "torchlike",
	paramtype = "light",
	tiles = { "testnodes_torchlike_floor.png^[colorize:#FF0000:64" },


	walkable = false,
	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:torchlike_wallmounted", {
	description = S("Wallmounted \"torchlike\" Drawtype Test Node").."\n"..
		S("param2 = wallmounted rotation (0..7)"),
	drawtype = "torchlike",
	paramtype = "light",
	paramtype2 = "wallmounted",
	tiles = {
		"testnodes_torchlike_floor.png",
		"testnodes_torchlike_ceiling.png",
		"testnodes_torchlike_wall.png",
	},


	walkable = false,
	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:torchlike_wallmounted_rot", {
	description = S("Wallmounted Rotatable Torchlike Drawtype Test Node"),
	drawtype = "torchlike",
	paramtype = "light",
	paramtype2 = "wallmounted",
	wallmounted_rotate_vertical = true,
	tiles = {
		"testnodes_torchlike_floor.png^[colorize:#FFFF00:40",
		"testnodes_torchlike_ceiling.png^[colorize:#FFFF00:40",
		"testnodes_torchlike_wall.png^[colorize:#FFFF00:40",
	},


	walkable = false,
	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:signlike", {
	description = S("Floor \"signlike\" Drawtype Test Node").."\n"..
		S("Always on floor"),
	drawtype = "signlike",
	paramtype = "light",
	tiles = { "testnodes_signlike.png^[colorize:#FF0000:64" },

	walkable = false,
	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:signlike_wallmounted", {
	description = S("Wallmounted \"signlike\" Drawtype Test Node").."\n"..
		S("param2 = wallmounted rotation (0..7)"),
	drawtype = "signlike",
	paramtype = "light",
	paramtype2 = "wallmounted",
	tiles = { "testnodes_signlike.png" },


	walkable = false,
	groups = { dig_immediate = 3 },
	sunlight_propagates = true,
})

core.register_node("testnodes:signlike_rot", {
	description = S("Wallmounted Rotatable Signlike Drawtype Test Node"),
	drawtype = "signlike",
	paramtype = "light",
	paramtype2 = "wallmounted",
	wallmounted_rotate_vertical = true,
	tiles = { "testnodes_signlike.png^[colorize:#FFFF00:40" },


	walkable = false,
	groups = { dig_immediate = 3 },
	sunlight_propagates = true,
})



core.register_node("testnodes:plantlike", {
	description = S("\"plantlike\" Drawtype Test Node"),
	drawtype = "plantlike",
	paramtype = "light",
	tiles = { "testnodes_plantlike.png" },


	walkable = false,
	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:plantlike_waving", {
	description = S("Waving \"plantlike\" Drawtype Test Node").."\n"..
		S("Waves if waving plants are enabled by client"),
	drawtype = "plantlike",
	paramtype = "light",
	tiles = { "testnodes_plantlike_waving.png" },
	waving = 1,


	walkable = false,
	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:plantlike_wallmounted", {
	description = S("Wallmounted \"plantlike\" Drawtype Test Node").."\n"..
		S("param2 = wallmounted rotation (0..7)"),
	drawtype = "plantlike",
	paramtype = "light",
	paramtype2 = "wallmounted",
	tiles = { "testnodes_plantlike_wallmounted.png" },
	leveled = 1,


	walkable = false,
	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})


-- param2 will rotate
local function rotate_on_rightclick(pos, node, clicker)
	local def = core.registered_nodes[node.name]
	local aux1 = clicker:get_player_control().aux1

	local deg, deg_max
	local color, color_mult = 0, 0
	if def.paramtype2 == "degrotate" then
		deg = node.param2
		deg_max = 240
	elseif def.paramtype2 == "colordegrotate" then
		-- MSB [3x color, 5x rotation] LSB
		deg = node.param2 % 2^5
		deg_max = 24
		color_mult = 2^5
		color = math.floor(node.param2 / color_mult)
	end

	deg = (deg + (aux1 and 10 or 1)) % deg_max
	node.param2 = color * color_mult + deg
	core.swap_node(pos, node)
	core.chat_send_player(clicker:get_player_name(),
		"Rotation is now " .. deg .. " / " .. deg_max)
end

core.register_node("testnodes:plantlike_degrotate", {
	description = S("Degrotate \"plantlike\" Drawtype Test Node").."\n"..
		S("param2 = horizontal rotation (0..239)"),
	drawtype = "plantlike",
	paramtype = "light",
	paramtype2 = "degrotate",
	tiles = { "testnodes_plantlike_degrotate.png" },

	on_rightclick = rotate_on_rightclick,
	place_param2 = 7,
	walkable = false,
	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:mesh_degrotate", {
	description = S("Degrotate \"mesh\" Drawtype Test Node").."\n"..
		S("param2 = horizontal rotation (0..239)"),
	drawtype = "mesh",
	paramtype = "light",
	paramtype2 = "degrotate",
	mesh = "testnodes_ocorner.obj",
	tiles = { "testnodes_mesh_stripes7.png" },

	on_rightclick = rotate_on_rightclick,
	place_param2 = 10, -- 15°
	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:mesh_colordegrotate", {
	description = S("Colordegrotate \"mesh\" Drawtype Test Node").."\n"..
		S("param2 = color + horizontal rotation (0..23, 32..55, ...)"),
	drawtype = "mesh",
	paramtype = "light",
	paramtype2 = "colordegrotate",
	palette = "testnodes_palette_facedir.png",
	mesh = "testnodes_ocorner.obj",
	tiles = { "testnodes_mesh_stripes8.png" },

	on_rightclick = rotate_on_rightclick,
	-- color index 1, 1 step (=15°) rotated
	place_param2 = 1 * 2^5 + 1,
	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})

-- param2 will change height
core.register_node("testnodes:plantlike_leveled", {
	description = S("Leveled \"plantlike\" Drawtype Test Node").."\n"..
		S("param2 = height (0..255)"),
	drawtype = "plantlike",
	paramtype = "light",
	paramtype2 = "leveled",
	tiles = {
		{ name = "testnodes_plantlike_leveled.png", tileable_vertical = true },
	},


	-- We set a default param2 here only for convenience, to make the "plant" visible after placement
	place_param2 = 8,
	walkable = false,
	sunlight_propagates = true,
	groups = { dig_immediate = 3 },
})

-- param2 changes shape
core.register_node("testnodes:plantlike_meshoptions", {
	description = S("Meshoptions \"plantlike\" Drawtype Test Node").."\n"..
		S("param2 = plant shape"),
	drawtype = "plantlike",
	paramtype = "light",
	paramtype2 = "meshoptions",
	tiles = { "testnodes_plantlike_meshoptions.png" },


	walkable = false,
	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:plantlike_rooted", {
	description = S("\"rooted_plantlike\" Drawtype Test Node"),
	drawtype = "plantlike_rooted",
	paramtype = "light",
	tiles = { "testnodes_plantlike_rooted_base.png" },
	special_tiles = { "testnodes_plantlike_rooted.png" },

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:plantlike_rooted_wallmounted", {
	description = S("Wallmounted \"rooted_plantlike\" Drawtype Test Node").."\n"..
		S("param2 = wallmounted rotation (0..7)"),
	drawtype = "plantlike_rooted",
	paramtype = "light",
	paramtype2 = "wallmounted",
	tiles = {
		"testnodes_plantlike_rooted_base.png",
		"testnodes_plantlike_rooted_base.png",
		"testnodes_plantlike_rooted_base_side_wallmounted.png" },
	special_tiles = { "testnodes_plantlike_rooted_wallmounted.png" },

	groups = { dig_immediate = 3 },
})

core.register_node("testnodes:plantlike_rooted_waving", {
	description = S("Waving \"rooted_plantlike\" Drawtype Test Node").."\n"..
		S("Waves if waving plants are enabled by client"),
	drawtype = "plantlike_rooted",
	paramtype = "light",
	tiles = {
		"testnodes_plantlike_rooted_base.png",
		"testnodes_plantlike_rooted_base.png",
		"testnodes_plantlike_rooted_base_side_waving.png",
	},
	special_tiles = { "testnodes_plantlike_rooted_waving.png" },
	waving = 1,

	groups = { dig_immediate = 3 },
})

-- param2 changes height
core.register_node("testnodes:plantlike_rooted_leveled", {
	description = S("Leveled \"rooted_plantlike\" Drawtype Test Node").."\n"..
		S("param2 = height (0..255)"),
	drawtype = "plantlike_rooted",
	paramtype = "light",
	paramtype2 = "leveled",
	tiles = {
		"testnodes_plantlike_rooted_base.png",
		"testnodes_plantlike_rooted_base.png",
		"testnodes_plantlike_rooted_base_side_leveled.png",
	},
	special_tiles = {
		{ name = "testnodes_plantlike_rooted_leveled.png", tileable_vertical = true },
	},


	-- We set a default param2 here only for convenience, to make the "plant" visible after placement
	place_param2 = 8,
	groups = { dig_immediate = 3 },
})

-- param2 changes shape
core.register_node("testnodes:plantlike_rooted_meshoptions", {
	description = S("Meshoptions \"rooted_plantlike\" Drawtype Test Node").."\n"..
		S("param2 = plant shape"),
	drawtype = "plantlike_rooted",
	paramtype = "light",
	paramtype2 = "meshoptions",
	tiles = {
		"testnodes_plantlike_rooted_base.png",
		"testnodes_plantlike_rooted_base.png",
		"testnodes_plantlike_rooted_base_side_meshoptions.png",
	},
	special_tiles = {
		"testnodes_plantlike_rooted_meshoptions.png",
	},

	groups = { dig_immediate = 3 },
})

-- param2 changes rotation
core.register_node("testnodes:plantlike_rooted_degrotate", {
	description = S("Degrotate \"rooted_plantlike\" Drawtype Test Node").."\n"..
		S("param2 = horizontal rotation (0..239)"),
	drawtype = "plantlike_rooted",
	paramtype = "light",
	paramtype2 = "degrotate",
	tiles = {
		"testnodes_plantlike_rooted_base.png",
		"testnodes_plantlike_rooted_base.png",
		"testnodes_plantlike_rooted_base_side_degrotate.png",
	},
	special_tiles = {
		"testnodes_plantlike_rooted_degrotate.png",
	},

	groups = { dig_immediate = 3 },
})

-- Demonstrative liquid nodes, source and flowing form.
-- DRAWTYPE ONLY, NO LIQUID PHYSICS!
-- Liquid ranges 0 to 8
for r = 0, 8 do
	core.register_node("testnodes:liquid_"..r, {
		description = S("\"liquid\" Drawtype Test Node, Range @1", r).."\n"..
			S("Drawtype only; all liquid physics are disabled"),
		drawtype = "liquid",
		paramtype = "light",
		tiles = {
			"testnodes_liquidsource_r"..r..".png^[colorize:#FFFFFF:100",
		},
		special_tiles = {
			{name="testnodes_liquidsource_r"..r..".png^[colorize:#FFFFFF:100", backface_culling=false},
			{name="testnodes_liquidsource_r"..r..".png^[colorize:#FFFFFF:100", backface_culling=true},
		},
		use_texture_alpha = "blend",


		walkable = false,
		liquid_range = r,
		liquid_viscosity = 0,
		liquid_alternative_flowing = "testnodes:liquid_flowing_"..r,
		liquid_alternative_source = "testnodes:liquid_"..r,
		groups = { dig_immediate = 3 },
	})
	core.register_node("testnodes:liquid_flowing_"..r, {
		description = S("\"flowingliquid\" Drawtype Test Node, Range @1", r).."\n"..
			S("Drawtype only; all liquid physics are disabled").."\n"..
			S("param2 = flowing liquid level"),
		drawtype = "flowingliquid",
		paramtype = "light",
		paramtype2 = "flowingliquid",
		tiles = {
			"testnodes_liquidflowing_r"..r..".png^[colorize:#FFFFFF:100",
		},
		special_tiles = {
			{name="testnodes_liquidflowing_r"..r..".png^[colorize:#FFFFFF:100", backface_culling=false},
			{name="testnodes_liquidflowing_r"..r..".png^[colorize:#FFFFFF:100", backface_culling=false},
		},
		use_texture_alpha = "blend",


		walkable = false,
		liquid_range = r,
		liquid_viscosity = 0,
		liquid_alternative_flowing = "testnodes:liquid_flowing_"..r,
		liquid_alternative_source = "testnodes:liquid_"..r,
		groups = { dig_immediate = 3 },
	})

end

-- Waving liquid test (drawtype only)
core.register_node("testnodes:liquid_waving", {
	description = S("Waving \"liquid\" Drawtype Test Node").."\n"..
		S("Drawtype only; all liquid physics are disabled").."\n"..
		S("Waves if waving liquids are enabled by client"),
	drawtype = "liquid",
	paramtype = "light",
	tiles = {
		"testnodes_liquidsource.png^[colorize:#0000FF:127",
	},
	special_tiles = {
		{name="testnodes_liquidsource.png^[colorize:#0000FF:127", backface_culling=false},
		{name="testnodes_liquidsource.png^[colorize:#0000FF:127", backface_culling=true},
	},
	use_texture_alpha = "blend",
	waving = 3,


	walkable = false,
	liquid_range = 1,
	liquid_viscosity = 0,
	liquid_alternative_flowing = "testnodes:liquid_flowing_waving",
	liquid_alternative_source = "testnodes:liquid_waving",
	groups = { dig_immediate = 3 },
})
core.register_node("testnodes:liquid_flowing_waving", {
	description = S("Waving \"flowingliquid\" Drawtype Test Node").."\n"..
		S("Drawtype only; all liquid physics are disabled").."\n"..
		S("param2 = flowing liquid level").."\n"..
		S("Waves if waving liquids are enabled by client"),
	drawtype = "flowingliquid",
	paramtype = "light",
	paramtype2 = "flowingliquid",
	tiles = {
		"testnodes_liquidflowing.png^[colorize:#0000FF:127",
	},
	special_tiles = {
		{name="testnodes_liquidflowing.png^[colorize:#0000FF:127", backface_culling=false},
		{name="testnodes_liquidflowing.png^[colorize:#0000FF:127", backface_culling=false},
	},
	use_texture_alpha = "blend",
	waving = 3,


	walkable = false,
	liquid_range = 1,
	liquid_viscosity = 0,
	liquid_alternative_flowing = "testnodes:liquid_flowing_waving",
	liquid_alternative_source = "testnodes:liquid_waving",
	groups = { dig_immediate = 3 },
})

-- Invisible node
core.register_node("testnodes:airlike", {
	description = S("\"airlike\" Drawtype Test Node").."\n"..
		S("Invisible node").."\n"..
		S("Inventory/wield image = no_texture_airlike.png"),
	drawtype = "airlike",
	paramtype = "light",
	-- inventory/wield images are left empty to make sure the 'no texture'
	-- fallback for airlike nodes is working properly.


	walkable = false,
	groups = { dig_immediate = 3 },
	sunlight_propagates = true,
})

-- param2 changes liquid height
core.register_node("testnodes:glassliquid", {
	description = S("\"glasslike_framed\" Drawtype with Liquid Test Node").."\n"..
		S("param2 = liquid level (0..63)"),
	drawtype = "glasslike_framed",
	paramtype = "light",
	paramtype2 = "glasslikeliquidlevel",
	tiles = {
		"testnodes_glasslikeliquid.png",
	},
	special_tiles = {
		"testnodes_liquid.png",
	},

	groups = { dig_immediate = 3 },
})

-- Adding many raillike examples, primarily to demonstrate the behavior of
-- "raillike groups". Nodes of the same type (rail, groupless, line, street)
-- should connect to nodes of the same "rail type" (=same shape, different
-- color) only.
local rails = {
	{ "rail", {"testnodes_rail_straight.png", "testnodes_rail_curved.png", "testnodes_rail_t_junction.png", "testnodes_rail_crossing.png"}, S("Connects to rails")},
	{ "line", {"testnodes_line_straight.png", "testnodes_line_curved.png", "testnodes_line_t_junction.png", "testnodes_line_crossing.png"}, S("Connects to lines")},
	{ "street", {"testnodes_street_straight.png", "testnodes_street_curved.png", "testnodes_street_t_junction.png", "testnodes_street_crossing.png"}, S("Connects to streets")},
	-- the "groupless" nodes are nodes in which the "connect_to_raillike" group is not set
	{ "groupless", {"testnodes_rail2_straight.png", "testnodes_rail2_curved.png", "testnodes_rail2_t_junction.png", "testnodes_rail2_crossing.png"}, S("Connects to 'groupless' rails") },
}
local colors = { "", "cyan", "red" }

for r=1, #rails do
	local id = rails[r][1]
	local tiles = rails[r][2]
	local tt = rails[r][3]
	local raillike_group
	if id ~= "groupless" then
		raillike_group = core.raillike_group(id)
	end
	for c=1, #colors do
		local color
		if colors[c] ~= "" then
			color = colors[c]
		end
		core.register_node("testnodes:raillike_"..id..c, {
			description = S("\"raillike\" Drawtype Test Node: @1 @2", id, c).."\n"..
				tt,
			drawtype = "raillike",
			paramtype = "light",
			tiles = tiles,
			groups = { connect_to_raillike = raillike_group, dig_immediate = 3 },


			color = color,
			selection_box = {
				type = "fixed",
				fixed = {{-0.5,  -0.5,  -0.5, 0.5, -0.4, 0.5}},
			},
			sunlight_propagates = true,
			walkable = false,
		})
	end
end



-- Add visual_scale variants of previous nodes for half and double size
local scale = function(subname, append)
	local original = "testnodes:"..subname
	local def = table.copy(core.registered_items[original])
	local orig_desc
	if append and type(append) == "string" then
		orig_desc = ItemStack(original):get_short_description()
		orig_desc = orig_desc .. "\n" .. append
	elseif append ~= false then
		orig_desc = ItemStack(original):get_description()
	else
		orig_desc = ItemStack(original):get_short_description()
	end
	def.visual_scale = 2.0
	def.description = S("Double-sized @1", orig_desc)
	core.register_node("testnodes:"..subname.."_double", def)
	def = table.copy(core.registered_items[original])
	def.visual_scale = 0.5
	def.description = S("Half-sized @1", orig_desc)
	core.register_node("testnodes:"..subname.."_half", def)
end

local allfaces_newsize_tt = ""..
	S("Rendering depends on 'leaves_style' setting:").."\n"..
	S("* 'fancy'/'simple': transparent").."\n"..
	S("* 'opaque': opaque")

scale("allfaces", S("Transparent node"))
scale("allfaces_optional", allfaces_newsize_tt)
scale("allfaces_optional_waving", allfaces_newsize_tt .. "\n" .. S("Waving if waving leaves are enabled by client"))
scale("plantlike")
scale("plantlike_wallmounted")
scale("torchlike_wallmounted")
scale("signlike_wallmounted")
scale("firelike")
