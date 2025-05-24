local function color(hex)
	return ("blank.png^[noalpha^[colorize:#%06X:255"):format(hex)
end

local function random_color()
	return color(math.random(0, 0xFFFFFF))
end

local function random_rotation()
	return 2 * math.pi * vector.new(math.random(), math.random(), math.random())
end

local active_selectionbox_entities = 0 -- count active entities

core.register_entity("testentities:selectionbox", {
	initial_properties = {
		visual = "cube",
		infotext = "Punch to randomize rotation, rightclick to toggle rotation"
	},
	on_activate = function(self)
		active_selectionbox_entities = active_selectionbox_entities + 1

		local w, h, l = math.random(), math.random(), math.random()
		self.object:set_properties({
			textures = {random_color(), random_color(), random_color(), random_color(), random_color(), random_color()},
			selectionbox = {rotate = true, -w/2, -h/2, -l/2, w/2, h/2, l/2},
			visual_size = vector.new(w, h, l),
			automatic_rotate = 2 * math.pi * (math.random() - 0.5)
		})
		assert(self.object:get_properties().selectionbox.rotate)
		self.object:set_armor_groups({punch_operable = 1})
		self.object:set_rotation(random_rotation())
	end,
	on_deactivate = function()
		active_selectionbox_entities = active_selectionbox_entities - 1
	end,
	on_punch = function(self)
		self.object:set_rotation(random_rotation())
	end,
	on_rightclick = function(self)
		self.object:set_properties({
			automatic_rotate = self.object:get_properties().automatic_rotate == 0 and 2 * math.pi * (math.random() - 0.5) or 0
		})
	end
})

local hud_ids = {}
core.register_globalstep(function()
	if active_selectionbox_entities == 0 then
		return
	end

	for _, player in pairs(core.get_connected_players()) do
		local offset = player:get_eye_offset()
		offset.y = offset.y + player:get_properties().eye_height
		local pos1 = vector.add(player:get_pos(), offset)
		local raycast = core.raycast(pos1, vector.add(pos1, vector.multiply(player:get_look_dir(), 10)), true, false)
		local pointed_thing = raycast()
		if pointed_thing.ref == player then
			pointed_thing = raycast()
		end
		local remove_hud_element = true
		local pname = player:get_player_name()
		local hud_id = hud_ids[pname]
		if pointed_thing and pointed_thing.type == "object" then
			local ent = pointed_thing.ref:get_luaentity()
			if ent and ent.name == "testentities:selectionbox" then
				hud_ids[pname] = hud_id or player:hud_add({
					type = "text",  -- See HUD element types
					position = {x=0.5, y=0.5},
					text = "X",
					number = 0xFF0000,
					alignment = {x=0, y=0},
				})
				local shade = math.random(0, 0xFF)
				core.add_particle({
					-- Random shade of red for the intersection point
					texture = color(0x10000 * shade),
					pos = pointed_thing.intersection_point,
					size = 0.1
				})
				core.add_particle({
					-- Same shade of green for the corresponding intersection normal
					texture = color(0x100 * shade),
					pos = vector.add(pointed_thing.intersection_point, pointed_thing.intersection_normal * 0.1),
					size = 0.1
				})
				remove_hud_element = false
			end
		end
		if remove_hud_element and hud_id then
			player:hud_remove(hud_id)
			hud_ids[pname] = nil
		end
	end
end)
