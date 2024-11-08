local simple_nodes = {
	footstep = { "Footstep Sound Node", "soundstuff_node_footstep.png", "Plays sound when you step on it" },
	dig = { "Dig Sound Node", "soundstuff_node_dig.png", "Plays sound when you dig it" },
	dug = { "Dug Sound Node", "soundstuff_node_dug.png", "Plays sound when you've dug it" },
	place = { "Place Sound Node", "soundstuff_node_place.png", "Plays sound when you place it" },
	place_failed = { "Place Failed Sound Node", "soundstuff_node_place_failed.png", "Plays sound when you try to place it but failed" },
}

for k,v in pairs(simple_nodes) do
	core.register_node("soundstuff:"..k, {
		description = v[1].."\n"..v[3],
		tiles = {"soundstuff_node_sound.png","soundstuff_node_sound.png",v[2]},
		groups = {dig_immediate=2},
		sounds = {
			[k] = { name = "soundstuff_mono", gain = 1.0 },
		}
	})
end

core.register_node("soundstuff:place_failed_attached", {
	description = "Attached Place Failed Sound Node".."\n"..
		"Attached to the floor; plays a sound when you try to place it but failed",
	tiles = {"soundstuff_node_sound.png", "soundstuff_node_sound.png", "soundstuff_node_place_failed.png"},
	groups = {dig_immediate=2, attached_node=1},
	drawtype = "nodebox",
	paramtype = "light",
	node_box = { type = "fixed", fixed = {
		{ -7/16, -7/16, -7/16, 7/16, 7/16, 7/16 },
		{ -0.5, -0.5, -0.5, 0.5, -7/16, 0.5 },
	}},
	sounds = {
		place_failed = { name = "soundstuff_mono", gain = 1.0 },
	},
})

core.register_node("soundstuff:fall", {
	description = "Fall Sound Node".."\n"..
		"Falls and plays sound if node below is gone",
	tiles = {"soundstuff_node_sound.png", "soundstuff_node_sound.png", "soundstuff_node_fall.png"},
	groups = {dig_immediate=2, falling_node=1},
	sounds = {
		fall = { name = "soundstuff_mono", gain = 1.0 },
	}
})

core.register_node("soundstuff:fall_attached", {
	description = "Attached Fall Sound Node".."\n"..
		"Drops as item and plays sound if node below is gone",
	tiles = {"soundstuff_node_sound.png", "soundstuff_node_sound.png", "soundstuff_node_fall.png"},
	groups = {dig_immediate=2, attached_node=1},
	drawtype = "nodebox",
	paramtype = "light",
	node_box = { type = "fixed", fixed = {
		{ -7/16, -7/16, -7/16, 7/16, 7/16, 7/16 },
		{ -0.5, -0.5, -0.5, 0.5, -7/16, 0.5 },
	}},
	sounds = {
		fall = { name = "soundstuff_mono", gain = 1.0 },
	}
})

core.register_node("soundstuff:footstep_liquid", {
	description = "Liquid Footstep Sound Node".."\n"..
		"Plays sound when moving inside it; swimmable",
	drawtype = "liquid",
	tiles = {
		"soundstuff_node_sound.png^[colorize:#0000FF:127^[opacity:190",
	},
	special_tiles = {
		{name = "soundstuff_node_sound.png^[colorize:#0000FF:127^[opacity:190",
			backface_culling = false},
		{name = "soundstuff_node_sound.png^[colorize:#0000FF:127^[opacity:190",
			backface_culling = true},
	},
	liquids_pointable = true,
	liquidtype = "source",
	liquid_alternative_flowing = "soundstuff:footstep_liquid",
	liquid_alternative_source = "soundstuff:footstep_liquid",
	liquid_renewable = false,
	liquid_range = 0,
	liquid_viscosity = 0,
	use_texture_alpha = "blend",
	paramtype = "light",
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	is_ground_content = false,
	post_effect_color = {a = 64, r = 0, g = 0, b = 200},
	sounds = {
		footstep = { name = "soundstuff_mono", gain = 1.0 },
	}
})

core.register_node("soundstuff:footstep_climbable", {
	description = "Climbable Footstep Sound Node".."\n"..
		"Plays sound when moving inside it; can climb up and down here",
	drawtype = "allfaces",
	tiles = {
		"soundstuff_node_climbable.png",
	},
	paramtype = "light",
	sunlight_propagates = true,
	walkable = false,
	climbable = true,
	is_ground_content = false,
	groups = { dig_immediate = 2 },
	sounds = {
		footstep = { name = "soundstuff_mono", gain = 1.0 },
	}
})



core.register_craftitem("soundstuff:eat", {
	description = "Eat Sound Item".."\n"..
		"Makes a sound when 'eaten' (with punch key)",
	inventory_image = "soundstuff_eat.png",
	on_use = core.item_eat(0),
	sound = {
		eat = { name = "soundstuff_mono", gain = 1.0 },
	}
})

core.register_tool("soundstuff:breaks", {
	description = "Break Sound Tool".."\n"..
		"Digs cracky=3 and more".."\n"..
		"Makes a sound when it breaks",
	inventory_image = "soundstuff_node_dug.png",
	sound = {
		breaks = { name = "soundstuff_mono", gain = 1.0 },
	},
	tool_capabilities = {
		max_drop_level=0,
		groupcaps={
			cracky={times={[2]=2.00, [3]=1.20}, uses=1, maxlevel=0},
			choppy={times={[2]=2.00, [3]=1.20}, uses=1, maxlevel=0},
			snappy={times={[2]=2.00, [3]=1.20}, uses=1, maxlevel=0},
			crumbly={times={[2]=2.00, [3]=1.20}, uses=1, maxlevel=0},
		},
	},
})


core.register_tool("soundstuff:punch_use", {
	description = "Punch Use Sound Tool\n"..
		"Digs cracky=3 and more\n"..
		"Makes a sound when used on node or entity",
	inventory_image = "soundstuff_node_dig.png",
	sound = {
		punch_use = { name = "soundstuff_mono", gain = 1.0 },
	},
	tool_capabilities = {
		max_drop_level=0,
		groupcaps={
			cracky={times={[2]=2.00, [3]=1.20}, uses=0, maxlevel=0},
			choppy={times={[2]=2.00, [3]=1.20}, uses=0, maxlevel=0},
			snappy={times={[2]=2.00, [3]=1.20}, uses=0, maxlevel=0},
			crumbly={times={[2]=2.00, [3]=1.20}, uses=0, maxlevel=0},
		},
	},
})

core.register_tool("soundstuff:punch_use_air", {
	description = "Punch Use (Air) Sound Tool\n"..
		"Makes a sound when used pointing at nothing",
	inventory_image = "soundstuff_node_dig.png",
	sound = {
		punch_use_air = { name = "soundstuff_mono", gain = 1.0 },
	},
})

-- Plays sound repeatedly
core.register_node("soundstuff:positional", {
	description = "Positional Sound Node".."\n"..
		"Repeatedly plays a sound at the node location",
	on_construct = function(pos)
		local timer = core.get_node_timer(pos)
		timer:start(0)
	end,
	on_timer = function(pos, elapsed)
		local node = core.get_node(pos)
		local dist = node.param2
		if dist == 0 then
			dist = nil
		end
		core.sound_play("soundstuff_mono", { pos = pos, max_hear_distance = dist })
		local timer = core.get_node_timer(pos)
		timer:start(0.7)
	end,
	on_rightclick = function(pos, node, clicker)
		node.param2 = (node.param2 + 1) % 64
		core.set_node(pos, node)
		if clicker and clicker:is_player() then
			local dist = node.param2
			local diststr
			if dist == 0 then
				diststr = "<default>"
			else
				diststr = tostring(dist)
			end
			core.chat_send_player(clicker:get_player_name(), "max_hear_distance = " .. diststr)
		end
	end,

	groups = { dig_immediate = 2 },
	tiles = { "soundstuff_node_sound.png" },
})

