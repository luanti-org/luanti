
local walk_speed = 2
local walk_distance = 10

core.register_entity("soundstuff:bigfoot", {
	initial_properties = {
		physical = false,
		collisionbox = {-1, -1, -1, 1, 1, 1},
		selectionbox = {-1, -1, -1, 1, 1, 1},
		visual = "upright_sprite",
		visual_size = {x = 2, y = 2, z = 2},
		textures = {"soundstuff_bigfoot.png", "soundstuff_bigfoot.png^[transformFX"},
		makes_footstep_sound = true,
		static_save = false,
	},

	on_activate = function(self, _staticdata, _dtime_s)
		self.min_x = self.object:get_pos().x - walk_distance * 0.5
		self.max_x = self.min_x + walk_distance
		self.vel = vector.new(walk_speed, 0, 0)
	end,

	on_step = function(self, _dtime, _moveresult)
		local pos = self.object:get_pos()
		if pos.x < self.min_x then
			self.vel = vector.new(walk_speed, 0, 0)
		elseif pos.x > self.max_x then
			self.vel = vector.new(-walk_speed, 0, 0)
		end
		self.object:set_velocity(self.vel)
	end,
})

core.register_chatcommand("spawn_bigfoot", {
	params = "",
	description = "Spawn a big foot object that makes footstep sounds",
	func = function(name, _param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local pos = player:get_pos()
		pos.y = pos.y + player:get_properties().collisionbox[2]
		pos.y = pos.y - (-1) -- bigfoot collisionbox goes 1 down
		core.add_entity(pos, "soundstuff:bigfoot")
		return true
	end,
})
