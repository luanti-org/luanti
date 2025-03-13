local S = core.get_translator("testnodes")

-- Nodebox examples and tests.

-- An simple example nodebox with one centered box
core.register_node("testnodes:nodebox_fixed", {
	description = S("Fixed Nodebox Test Node").."\n"..
		S("Nodebox is always the same"),
	tiles = {"testnodes_nodebox.png"},
	drawtype = "nodebox",
	paramtype = "light",
	node_box = {
		type = "fixed",
		fixed = {-0.25, -0.25, -0.25, 0.25, 0.25, 0.25},
	},

	groups = {dig_immediate=3},
})

-- 50% higher than a regular node
core.register_node("testnodes:nodebox_overhigh", {
	description = S("+50% high Nodebox Test Node"),
	tiles = {"testnodes_nodebox.png"},
	drawtype = "nodebox",
	paramtype = "light",
	node_box = {
		type = "fixed",
		fixed = {-0.5, -0.5, -0.5, 0.5, 1, 0.5},
	},

	groups = {dig_immediate=3},
})

-- 95% higher than a regular node
core.register_node("testnodes:nodebox_overhigh2", {
	description = S("+95% high Nodebox Test Node"),
	tiles = {"testnodes_nodebox.png"},
	drawtype = "nodebox",
	paramtype = "light",
	node_box = {
		type = "fixed",
		-- Y max: more is possible, but glitchy
		fixed = {-0.5, -0.5, -0.5, 0.5, 1.45, 0.5},
	},

	groups = {dig_immediate=3},
})

-- Height of nodebox changes with its param2 value
core.register_node("testnodes:nodebox_leveled", {
	description = S("Leveled Nodebox Test Node").."\n"..
		S("param2 = height (0..127)"),
	tiles = {"testnodes_nodebox.png^[colorize:#0F0:32"},
	drawtype = "nodebox",
	paramtype = "light",
	paramtype2 = "leveled",
	node_box = {
		type = "leveled",
		fixed = {-0.5, 0.0, -0.5, 0.5, -0.499, 0.5},
	},

	groups = {dig_immediate=3},
})


local nodebox_wall = {
	type = "connected",
	fixed = {-0.125, -0.500, -0.125, 0.125, 0.500, 0.125},
	connect_front = {-0.125, -0.500, -0.500, 0.125, 0.400, -0.125},
	connect_back = {-0.125, -0.500, 0.125, 0.125, 0.400, 0.500},
	connect_left = {-0.500, -0.500, -0.125, -0.125, 0.400, 0.125},
	connect_right = {0.125, -0.500, -0.125, 0.500, 0.400, 0.125},
}

local nodebox_cable = {
	type = "connected",
	fixed          = {-2/16, -2/16, -2/16,  2/16,  2/16,  2/16},
	connect_front  = {-1/16, -1/16, -8/16,  1/16,  1/16, -2/16},
	connect_back   = {-1/16, -1/16,  2/16,  1/16,  1/16,  8/16},
	connect_left   = {-8/16, -1/16, -1/16, -2/16,  1/16,  1/16},
	connect_right  = { 2/16, -1/16, -1/16,  8/16,  1/16,  1/16},
	connect_bottom = {-1/16, -8/16, -1/16,  1/16, -2/16,  1/16},
	connect_top    = {-1/16,  2/16, -1/16,  1/16,  8/16,  1/16},
}

local nodebox_wall_thick = {
	type = "connected",
	fixed = {-0.25, -0.500, -0.25, 0.25, 0.500, 0.25},
	connect_front = {-0.25, -0.500, -0.500, 0.25, 0.400, -0.25},
	connect_back = {-0.25, -0.500, 0.25, 0.25, 0.400, 0.500},
	connect_left = {-0.500, -0.500, -0.25, -0.25, 0.400, 0.25},
	connect_right = {0.25, -0.500, -0.25, 0.500, 0.400, 0.25},
}

-- Wall-like nodebox that connects to 4 neighbors
core.register_node("testnodes:nodebox_connected", {
	description = S("Connected Nodebox Test Node (4 Side Wall)").."\n"..
		S("Connects to 4 neighbors sideways"),
	tiles = {"testnodes_nodebox.png^[colorize:#F00:32"},
	groups = {connected_nodebox=1, dig_immediate=3},
	drawtype = "nodebox",
	paramtype = "light",
	connects_to = {"group:connected_nodebox"},
	connect_sides = {"front", "back", "left", "right"},
	node_box = nodebox_wall,
})

-- Cable-like nodebox that connects to 6 neighbors
core.register_node("testnodes:nodebox_connected_6side", {
	description = S("Connected Nodebox Test Node (6 Side Cable)").."\n"..
		S("Connects to 6 neighbors"),
	tiles = {"testnodes_nodebox.png^[colorize:#F00:32"},
	groups = {connected_nodebox=1, dig_immediate=3},
	drawtype = "nodebox",
	paramtype = "light",
	connects_to = {"group:connected_nodebox"},
	connect_sides = {"front", "back", "left", "right", "top", "bottom"},
	node_box = nodebox_cable,
})

-- More walls
core.register_node("testnodes:nodebox_connected_facedir", {
	description = S("Facedir Connected Nodebox Test Node (4 Side Wall)").."\n"..
		S("Connects to neighbors").."\n"..
		S("param2 = facedir rotation of textures (not of the nodebox!)"),
	tiles = {
		"testnodes_1.png",
		"testnodes_2.png",
		"testnodes_3.png",
		"testnodes_4.png",
		"testnodes_5.png",
		"testnodes_6.png",
	},
	groups = {connected_nodebox=1, dig_immediate=3},
	drawtype = "nodebox",
	paramtype = "light",
	paramtype2 = "facedir",
	connects_to = {"group:connected_nodebox"},
	connect_sides = {"front", "back", "left", "right"},
	node_box = nodebox_wall_thick,
})

core.register_node("testnodes:nodebox_connected_4dir", {
	description = S("4Dir Connected Nodebox Test Node").."\n"..
		S("Connects to neighbors").."\n"..
		S("param2 = 4dir rotation of textures (not of the nodebox!)"),
	tiles = {
		"testnodes_1f.png",
		"testnodes_2f.png",
		"testnodes_3f.png",
		"testnodes_4f.png",
		"testnodes_5f.png",
		"testnodes_6f.png",
	},
	groups = {connected_nodebox=1, dig_immediate=3},
	drawtype = "nodebox",
	paramtype = "light",
	paramtype2 = "4dir",
	connects_to = {"group:connected_nodebox"},
	connect_sides = {"front", "back", "left", "right"},
	node_box = nodebox_wall_thick,
})

-- Doesn't connect, but lets other nodes connect
core.register_node("testnodes:facedir_to_connect_to", {
	description = S("Facedir Node that connected Nodeboxes connect to").."\n"..
		S("Neighbors connect only to left (blue 4) and top (yellow 1) face").."\n"..
		S("(Currently broken for param2 >= 4, see FIXME in nodedef.cpp)").."\n"..
		S("param2 = facedir"),
	tiles = {
		"testnodes_1.png",
		"testnodes_2.png",
		"testnodes_3.png",
		"testnodes_4.png",
		"testnodes_5.png",
		"testnodes_6.png",
	},
	groups = {connected_nodebox=1, dig_immediate=3},
	drawtype = "normal",
	paramtype2 = "facedir",
	connect_sides = {"left", "top"},
})

-- 3D sign and button:
-- These are example nodes for more realistic example uses
-- of wallmounted_rotate_vertical
core.register_node("testnodes:sign3d", {
	description = S("Nodebox Sign, Nodebox Type \"fixed\""),
	drawtype = "nodebox",
	paramtype = "light",
	paramtype2 = "wallmounted",
	wallmounted_rotate_vertical = true,
	sunlight_propagates = true,
	walkable = false,
	tiles = {
		"testnodes_sign3d.png",
	},
	groups = { dig_immediate = 3 },
	node_box = {
		type = "fixed",
		fixed = {-0.4375, -0.5, -0.3125, 0.4375, -0.4375, 0.3125},
	},
})

core.register_node("testnodes:sign3d_wallmounted", {
	description = S("Nodebox Sign, Nodebox Type \"wallmounted\""),
	drawtype = "nodebox",
	paramtype = "light",
	paramtype2 = "wallmounted",
	wallmounted_rotate_vertical = true,
	sunlight_propagates = true,
	walkable = false,
	tiles = {
		"testnodes_sign3d.png^[colorize:#ff0000:127",
	},
	groups = { dig_immediate = 3 },
	node_box = {
		type = "wallmounted",
		wall_top    = {-0.4375, 0.4375, -0.3125, 0.4375, 0.5, 0.3125},
		wall_bottom = {-0.4375, -0.5, -0.3125, 0.4375, -0.4375, 0.3125},
		wall_side   = {-0.5, -0.3125, -0.4375, -0.4375, 0.3125, 0.4375},
	},
})

core.register_node("testnodes:button", {
	description = S("Button Nodebox Test Node"),
	drawtype = "nodebox",
	paramtype = "light",
	paramtype2 = "wallmounted",
	wallmounted_rotate_vertical = true,
	sunlight_propagates = true,
	walkable = false,
	tiles = {
		"testnodes_nodebox.png",
	},
	groups = { dig_immediate = 3 },
	node_box = {
		type = "fixed",
		fixed = { -4/16, -8/16, -2/16, 4/16, -6/16, 2/16 },
	},
})

