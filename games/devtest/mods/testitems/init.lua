local S = core.get_translator("testitems")

--
-- Texture overlays for items
--

-- For the global overlay color test
local GLOBAL_COLOR_ARG = "orange"

-- Punch handler to set random color with "color" argument in item metadata
local overlay_on_use = function(itemstack, user, pointed_thing)
	local meta = itemstack:get_meta()
	local color = math.random(0x0, 0xFFFFFF)
	local colorstr = string.format("#%06x", color)
	meta:set_string("color", colorstr)
	core.log("action", "[testitems] Color of "..itemstack:get_name().." changed to "..colorstr)
	return itemstack
end
-- Place handler to clear item metadata color
local overlay_on_place = function(itemstack, user, pointed_thing)
	local meta = itemstack:get_meta()
	meta:set_string("color", "")
	return itemstack
end

core.register_craftitem("testitems:overlay_meta", {
	description = S("Texture Overlay Test Item, Meta Color") .. "\n" ..
		S("Image must be a square with rainbow cross (inventory and wield)") .. "\n" ..
		S("Item meta color must only change square color") .. "\n" ..
		S("Punch: Set random color") .. "\n" ..
		S("Place: Clear color"),
	-- Base texture: A grayscale square (can be colorized)
	inventory_image = "testitems_overlay_base.png",
	wield_image = "testitems_overlay_base.png",
	-- Overlay: A rainbow cross (NOT to be colorized!)
	inventory_overlay = "testitems_overlay_overlay.png",
	wield_overlay = "testitems_overlay_overlay.png",

	on_use = overlay_on_use,
	on_place = overlay_on_place,
	on_secondary_use = overlay_on_place,
})

core.register_craftitem("testitems:overlay_global", {
	description = S("Texture Overlay Test Item, Global Color") .. "\n" ..
		S("Image must be an orange square with rainbow cross (inventory and wield)"),
	-- Base texture: A grayscale square (to be colorized)
	inventory_image = "testitems_overlay_base.png",
	wield_image = "testitems_overlay_base.png",
	-- Overlay: A rainbow cross (NOT to be colorized!)
	inventory_overlay = "testitems_overlay_overlay.png",
	wield_overlay = "testitems_overlay_overlay.png",
	color = GLOBAL_COLOR_ARG,
})

core.register_craftitem("testitems:overlay_size", {
	description = S("Texture Overlay Test Item, Big Base Small Overlay") .. "\n" ..
		S("Wield overlay should not be blurry."),
	inventory_image = "[fill:1000x1000:#FFFFFF",
	inventory_overlay = "testitems_overlay_overlay.png",
})

core.register_craftitem("testitems:image_meta", {
	description = S("Image Override Meta Test Item"),
	inventory_image = "default_apple.png",
	wield_image = "basetools_icesword.png",

	on_use = function(itemstack, player)
		local meta = itemstack:get_meta()
		local state = meta:get_int("state")
		state = (state + 1) % 5
		meta:set_int("state", state)
		core.chat_send_player(player:get_player_name(), "State " .. state)

		if state == 0 then
			meta:set_string("inventory_image", "")
			meta:set_string("wield_image", "")
			meta:set_string("inventory_overlay", "")
			meta:set_string("wield_overlay", "")
			meta:set_string("wield_scale", "")
		elseif state == 1 then
			meta:set_string("inventory_image", "default_tree.png")
			meta:set_string("wield_image", "basetools_firesword.png")
		elseif state == 2 then
			meta:set_string("inventory_image", "default_apple.png^testitems_overridden.png")
			meta:set_string("wield_image", "basetools_icesword.png^testitems_overridden.png")
		elseif state == 3 then
			meta:set_string("inventory_image", "default_tree.png")
			meta:set_string("wield_image", "basetools_firesword.png")
			meta:set_string("inventory_overlay", "default_apple.png")
			meta:set_string("wield_overlay", "default_apple.png")
		elseif state == 4 then
			local scale = vector.new(0.5, 0.5, 0.5)
			meta:set_string("wield_scale", scale:to_string())
		end

		return itemstack
	end,
})

core.register_craftitem("testitems:telescope_stick", {
	description = S("Telescope Stick (Increases range on use.)"),
	inventory_image = "testitems_telescope_stick.png",
	on_use = function(itemstack, player)
		local meta = itemstack:get_meta()
		local range = meta:get_float("range") + 1.2
		if range > 10 then
			range = 0
		end
		meta:set_float("range", range)
		core.chat_send_player(player:get_player_name(), "Telescope Stick range set to "..range)
		return itemstack
	end,
})


-- Tree spawners

local tree_def={
	axiom="Af",
	rules_a="TT[&GB][&+GB][&++GB][&+++GB]A",
	rules_b="[+GB]fB",
	trunk="basenodes:tree",
	leaves="basenodes:leaves",
	angle=90,
	iterations=4,
	trunk_type="single",
	thin_branches=true,
}

core.register_craftitem("testitems:tree_spawner", {
	description = S("Tree Spawner"),
	inventory_image = "testitems_tree_spawner.png",
	on_place = function(itemstack, placer, pointed_thing)
		if (not pointed_thing or pointed_thing.type ~= "node") then
			return
		end
		core.spawn_tree(pointed_thing.above, tree_def)
	end,
})

local vmanip_for_trees = {} -- per player
core.register_craftitem("testitems:tree_spawner_vmanip", {
	description = S("Tree Spawner using VoxelManip"),
	inventory_image = "testitems_tree_spawner_vmanip.png",
	on_place = function(itemstack, placer, pointed_thing)
		if (not pointed_thing or pointed_thing.type ~= "node" or
				not core.is_player(placer)) then
			return
		end
		local name = placer:get_player_name()
		local vm = vmanip_for_trees[name]
		if not vm then
			vm = VoxelManip(vector.add(pointed_thing.above, 20),
				vector.subtract(pointed_thing.above, 20))
			vmanip_for_trees[name] = vm
			core.chat_send_player(name,
				"Tree in new VoxelManip spawned, left click to apply to map, "..
				"or right click to add more trees.")
		end
		core.spawn_tree_on_vmanip(vm, pointed_thing.above, tree_def)
	end,
	on_use = function(itemstack, user, pointed_thing)
		if not core.is_player(user) then
			return
		end
		local name = user:get_player_name()
		local vm = vmanip_for_trees[name]
		if vm then
			vm:write_to_map()
			vmanip_for_trees[name] = nil
			core.chat_send_player(name, "VoxelManip written to map.")
		end
	end,
})

core.register_on_leaveplayer(function(player, timed_out)
	vmanip_for_trees[player:get_player_name()] = nil
end)

-- Animation test items

local animated_image = {
	name = "testnodes_anim.png^[invert:rgb",
	animation = {
		type = "vertical_frames",
		aspect_w = 16,
		aspect_h = 16,
		length = 4.0,
	}
}

local animated_overlay = {
	name = "testitems_animation_overlay.png",
	animation = {
		type = "sheet_2d",
		frames_w = 1,
		frames_h = 4,
		frame_length = 1.0,
	}
}

core.register_craftitem("testitems:inventory_image_animation", {
	description = S("Animated Test Item").."\n"..
		S("Image animate from A to D in 4s cycle"),
	inventory_image = animated_image
})

core.register_craftitem("testitems:inventory_image_animation_overlay", {
	description = S("Animated Test Item With Overlay").."\n"..
		S("Should be colored red and have green stripes that move").."\n"..
		S("Image animate from A to D in 4s cycle"),
	inventory_image = animated_image,
	inventory_overlay = animated_overlay,
	color = "#ff0000",
})

core.register_craftitem("testitems:wield_image_animation", {
	description = S("Wield Animated Test Item").."\n"..
		S("Looks like the Animated Test Item, "..
			"but only animated for the wield mesh."),
	inventory_image = "testnodes_anim.png^[invert:rgb^[verticalframe:4:0",
	wield_image = animated_image,
})

core.register_craftitem("testitems:wield_image_animation_overlay", {
	description = S("Wield Animated Test Item With Overlay").."\n"..
		S("Looks like the animated Test Item With Overlay, "..
			"but only animated for the wield mesh."),
	inventory_image = "testnodes_anim.png^[invert:rgb^[verticalframe:4:0",
	inventory_overlay = "testitems_animation_overlay.png^[verticalframe:4:0",
	wield_image = animated_image,
	wield_overlay = animated_overlay,
	color = "#ff0000",
})

local function animated_image_accelerated(speed)
	local image = table.copy(animated_image)
	image.animation.length = image.animation.length * speed
	return image
end

local function animated_overlay_accelerated(speed)
	local image = table.copy(animated_overlay)
	image.animation.frame_length = image.animation.frame_length * speed
	return image
end

local function update_animation(meta, mode, speed, name)
	local msg
	local speed_factor = 1/(speed+1)
	if mode == 0 then
		meta:set_wield_overlay()
		msg = S"Not animated"
	elseif mode == 1 then
		meta:set_inventory_image(animated_image_accelerated(speed_factor))
		msg = S"Inventory image animated"
	elseif mode == 2 then
		meta:set_inventory_image()
		meta:set_inventory_overlay(animated_overlay_accelerated(speed_factor))
		msg = S"Inventory overlay animated"
	elseif mode == 3 then
		meta:set_inventory_overlay()
		meta:set_wield_image(animated_image_accelerated(speed_factor))
		msg = S"Wield image animated"
	else
		meta:set_wield_image()
		meta:set_wield_overlay(animated_overlay_accelerated(speed_factor))
		msg = S"Wield overlay animated"
	end
	core.chat_send_player(name, msg .. S", speed " .. speed)
end

local function toggle_red_string_meta_inventory_image(itemstack)
	local meta = itemstack:get_meta()
	local image = meta:get_string("inventory_image") == "" and
			"testnodes_anim.png^[invert:rgb^[verticalframe:4:0^[multiply:#ff0000" or ""
	local overlay = meta:get_string("inventory_overlay") == "" and
			"testitems_animation_overlay.png^[verticalframe:4:0^[multiply:#ff0000" or ""
	meta:set_string("inventory_image", image)
	meta:set_string("inventory_overlay", overlay)
	meta:set_string("wield_image", image)
	meta:set_string("wield_overlay", overlay)
	return itemstack
end

core.register_craftitem("testitems:inventory_image_animation_meta", {
	description = S("Animated Meta Test Item").."\n"..
		S("Left click to cycle through modes").."\n"..
		S("Hold sneak and left click to change speed").."\n"..
		S("Right click to toggle red string meta inventory image fields"),
	inventory_image = "testnodes_anim.png^[invert:rgb^[verticalframe:4:0",
	inventory_overlay = "testitems_animation_overlay.png^[verticalframe:4:0",
	wield_image = "testnodes_anim.png^[invert:rgb^[verticalframe:4:0",
	wield_overlay = "testitems_animation_overlay.png^[verticalframe:4:0",
	on_use = function(itemstack, user, pointed_thing)
		if not core.is_player(user) then
			return
		end
		local name = user:get_player_name()
		local meta = itemstack:get_meta()
		local speed = meta:get_int("speed")
		local mode = meta:get_int("mode")

		if user:get_player_control().sneak then
			speed = (speed + 1) % 4
			meta:set_int("speed", speed)
		else
			mode = (mode + 1) % 5
			meta:set_int("mode", mode)
		end
		update_animation(meta, mode, speed, name)
		return itemstack
	end,
	on_place = toggle_red_string_meta_inventory_image,
	on_secondary_use = toggle_red_string_meta_inventory_image,
})
