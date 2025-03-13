local WATER_ALPHA = "^[opacity:" .. 160
local WATER_VISC = 1
local LAVA_VISC = 7

--
-- Node definitions
--

-- Register nodes

core.register_node("basenodes:stone", {
	description = "Stone",
	tiles = {"default_stone.png"},
	groups = {cracky=3},
})

core.register_node("basenodes:desert_stone", {
	description = "Desert Stone",
	tiles = {"default_desert_stone.png"},
	groups = {cracky=3},
})

core.register_node("basenodes:dirt_with_grass", {
	description = "Dirt with Grass",
	-- Using overlays here has no real merit here but we do it anyway so
	-- overlay-related bugs become more apparent in devtest.
	tiles = {"default_dirt.png"},
	overlay_tiles = {
		"default_grass.png",
		-- a little dot on the bottom to distinguish it from dirt
		"basenodes_dirt_with_grass_bottom.png",
		{name = "default_grass_side.png", tileable_vertical = false},
	},
	groups = {crumbly=3, soil=1},
})

core.register_node("basenodes:dirt_with_snow", {
	description = "Dirt with Snow",
	tiles ={"basenodes_dirt_with_snow.png",
		-- a little dot on the bottom to distinguish it from dirt
		"default_dirt.png^basenodes_dirt_with_snow_bottom.png",
		{name = "default_dirt.png^default_snow_side.png",
		tileable_vertical = false}},
	groups = {crumbly=3, soil=1},
})

core.register_node("basenodes:dirt", {
	description = "Dirt",
	tiles ={"default_dirt.png"},
	groups = {crumbly=3, soil=1},
})

core.register_node("basenodes:sand", {
	description = "Sand",
	tiles ={"default_sand.png"},
	groups = {crumbly=3},
})

core.register_node("basenodes:desert_sand", {
	description = "Desert Sand",
	tiles ={"default_desert_sand.png"},
	groups = {crumbly=3},
})

core.register_node("basenodes:gravel", {
	description = "Gravel",
	tiles ={"default_gravel.png"},
	groups = {crumbly=2},
})

core.register_node("basenodes:junglegrass", {
	description = "Jungle Grass",
	drawtype = "plantlike",
	tiles ={"default_junglegrass.png"},
	inventory_image = "default_junglegrass.png",
	wield_image = "default_junglegrass.png",
	paramtype = "light",
	walkable = false,
	groups = {snappy=3},
})

core.register_node("basenodes:tree", {
	description = "Normal Tree Trunk",
	tiles = {"default_tree_top.png", "default_tree_top.png", "default_tree.png"},
	is_ground_content = false,
	groups = {choppy=2,oddly_breakable_by_hand=1},
})

core.register_node("basenodes:leaves", {
	description = "Normal Leaves",
	drawtype = "allfaces_optional",
	tiles = {"default_leaves.png"},
	paramtype = "light",
	is_ground_content = false,
	groups = {snappy=3},
})

core.register_node("basenodes:jungletree", {
	description = "Jungle Tree Trunk",
	tiles = {"default_jungletree_top.png", "default_jungletree_top.png", "default_jungletree.png"},
	is_ground_content = false,
	groups = {choppy=2,oddly_breakable_by_hand=1},
})

core.register_node("basenodes:jungleleaves", {
	description = "Jungle Leaves",
	drawtype = "allfaces_optional",
	tiles = {"default_jungleleaves.png"},
	paramtype = "light",
	is_ground_content = false,
	groups = {snappy=3},
})

core.register_node("basenodes:pine_tree", {
	description = "Pine Tree Trunk",
	tiles = {"default_pine_tree_top.png", "default_pine_tree_top.png", "default_pine_tree.png"},
	is_ground_content = false,
	groups = {choppy=2,oddly_breakable_by_hand=1},
})

core.register_node("basenodes:pine_needles", {
	description = "Pine Needles",
	drawtype = "allfaces_optional",
	tiles = {"default_pine_needles.png"},
	paramtype = "light",
	is_ground_content = false,
	groups = {snappy=3},
})

core.register_node("basenodes:water_source", {
	description = "Water Source".."\n"..
		"Swimmable, spreading, renewable liquid".."\n"..
		"Drowning damage: 1",
	drawtype = "liquid",
	waving = 3,
	tiles = {"default_water.png"..WATER_ALPHA},
	special_tiles = {
		{name = "default_water.png"..WATER_ALPHA, backface_culling = false},
		{name = "default_water.png"..WATER_ALPHA, backface_culling = true},
	},
	use_texture_alpha = "blend",
	paramtype = "light",
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	is_ground_content = false,
	drowning = 1,
	liquidtype = "source",
	liquid_alternative_flowing = "basenodes:water_flowing",
	liquid_alternative_source = "basenodes:water_source",
	liquid_viscosity = WATER_VISC,
	post_effect_color = {a = 64, r = 100, g = 100, b = 200},
	post_effect_color_shaded = true,
	groups = {water = 3, liquid = 3},
})

core.register_node("basenodes:water_flowing", {
	description = "Flowing Water".."\n"..
		"Swimmable, spreading, renewable liquid".."\n"..
		"Drowning damage: 1",
	drawtype = "flowingliquid",
	waving = 3,
	tiles = {"default_water_flowing.png"},
	special_tiles = {
		{name = "default_water_flowing.png"..WATER_ALPHA,
			backface_culling = false},
		{name = "default_water_flowing.png"..WATER_ALPHA,
			backface_culling = false},
	},
	use_texture_alpha = "blend",
	paramtype = "light",
	paramtype2 = "flowingliquid",
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	is_ground_content = false,
	drowning = 1,
	liquidtype = "flowing",
	liquid_alternative_flowing = "basenodes:water_flowing",
	liquid_alternative_source = "basenodes:water_source",
	liquid_viscosity = WATER_VISC,
	post_effect_color = {a = 64, r = 100, g = 100, b = 200},
	post_effect_color_shaded = true,
	groups = {water = 3, liquid = 3},
})

core.register_node("basenodes:river_water_source", {
	description = "River Water Source".."\n"..
		"Swimmable, spreading, non-renewable liquid".."\n"..
		"Drowning damage: 1",
	drawtype = "liquid",
	waving = 3,
	tiles = { "default_river_water.png"..WATER_ALPHA },
	special_tiles = {
		{name = "default_river_water.png"..WATER_ALPHA, backface_culling = false},
		{name = "default_river_water.png"..WATER_ALPHA, backface_culling = true},
	},
	use_texture_alpha = "blend",
	paramtype = "light",
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	is_ground_content = false,
	drowning = 1,
	liquidtype = "source",
	liquid_alternative_flowing = "basenodes:river_water_flowing",
	liquid_alternative_source = "basenodes:river_water_source",
	liquid_viscosity = 1,
	liquid_renewable = false,
	liquid_range = 2,
	post_effect_color = {a = 103, r = 30, g = 76, b = 90},
	post_effect_color_shaded = true,
	groups = {water = 3, liquid = 3, },
})

core.register_node("basenodes:river_water_flowing", {
	description = "Flowing River Water".."\n"..
		"Swimmable, spreading, non-renewable liquid".."\n"..
		"Drowning damage: 1",
	drawtype = "flowingliquid",
	waving = 3,
	tiles = {"default_river_water_flowing.png"..WATER_ALPHA},
	special_tiles = {
		{name = "default_river_water_flowing.png"..WATER_ALPHA,
			backface_culling = false},
		{name = "default_river_water_flowing.png"..WATER_ALPHA,
			backface_culling = false},
	},
	use_texture_alpha = "blend",
	paramtype = "light",
	paramtype2 = "flowingliquid",
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	is_ground_content = false,
	drowning = 1,
	liquidtype = "flowing",
	liquid_alternative_flowing = "basenodes:river_water_flowing",
	liquid_alternative_source = "basenodes:river_water_source",
	liquid_viscosity = 1,
	liquid_renewable = false,
	liquid_range = 2,
	post_effect_color = {a = 103, r = 30, g = 76, b = 90},
	post_effect_color_shaded = true,
	groups = {water = 3, liquid = 3, },
})

core.register_node("basenodes:lava_flowing", {
	description = "Flowing Lava".."\n"..
		"Swimmable, spreading, renewable liquid".."\n"..
		"4 damage per second".."\n"..
		"Drowning damage: 1",
	drawtype = "flowingliquid",
	tiles = {"default_lava_flowing.png"},
	special_tiles = {
		{name="default_lava_flowing.png", backface_culling = false},
		{name="default_lava_flowing.png", backface_culling = false},
	},
	paramtype = "light",
	light_source = core.LIGHT_MAX,
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	is_ground_content = false,
	drowning = 1,
	damage_per_second = 4,
	liquidtype = "flowing",
	liquid_alternative_flowing = "basenodes:lava_flowing",
	liquid_alternative_source = "basenodes:lava_source",
	liquid_viscosity = LAVA_VISC,
	post_effect_color = {a=192, r=255, g=64, b=0},
	groups = {lava=3, liquid=1},
})

core.register_node("basenodes:lava_source", {
	description = "Lava Source".."\n"..
		"Swimmable, spreading, renewable liquid".."\n"..
		"4 damage per second".."\n"..
		"Drowning damage: 1",
	drawtype = "liquid",
	tiles = { "default_lava.png" },
	special_tiles = {
		{name = "default_lava.png", backface_culling = false},
		{name = "default_lava.png", backface_culling = true},
	},
	paramtype = "light",
	light_source = core.LIGHT_MAX,
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	is_ground_content = false,
	drowning = 1,
	damage_per_second = 4,
	liquidtype = "source",
	liquid_alternative_flowing = "basenodes:lava_flowing",
	liquid_alternative_source = "basenodes:lava_source",
	liquid_viscosity = LAVA_VISC,
	post_effect_color = {a=192, r=255, g=64, b=0},
	groups = {lava=3, liquid=1},
})

core.register_node("basenodes:cobble", {
	description = "Cobblestone",
	tiles ={"default_cobble.png"},
	is_ground_content = false,
	groups = {cracky=3},
})

core.register_node("basenodes:mossycobble", {
	description = "Mossy Cobblestone",
	tiles ={"default_mossycobble.png"},
	is_ground_content = false,
	groups = {cracky=3},
})

core.register_node("basenodes:apple", {
	description = "Apple".."\n"..
		"Punch: Eat (+2)",
	drawtype = "plantlike",
	tiles ={"default_apple.png"},
	inventory_image = "default_apple.png",
	paramtype = "light",
	is_ground_content = false,
	sunlight_propagates = true,
	walkable = false,
	groups = {dig_immediate=3},

	-- Make eatable because why not?
	on_use = core.item_eat(2),
})

core.register_node("basenodes:ice", {
	description = "Ice",
	tiles ={"default_ice.png"},
	groups = {cracky=3},
})

-- The snow nodes intentionally have different tints to make them more
-- distinguishable
core.register_node("basenodes:snow", {
	description = "Snow Sheet",
	tiles = {"basenodes_snow_sheet.png"},
	groups = {crumbly=3},
	walkable = false,
	paramtype = "light",
	drawtype = "nodebox",
	node_box = {
		type = "fixed",
		fixed = {-0.5, -0.5, -0.5, 0.5, -0.25, 0.5},
	},
})

core.register_node("basenodes:snowblock", {
	description = "Snow Block",
	tiles ={"default_snow.png"},
	groups = {crumbly=3},
})


