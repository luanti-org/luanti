
local drive_speed = 20
local drive_distance = 30

core.register_entity("soundstuff:racecar", {
	initial_properties = {
		physical = false,
		collisionbox = {-0.5, -0.5, -0.5, 0.5, 0.5, 0.5},
		selectionbox = {-0.5, -0.5, -0.5, 0.5, 0.5, 0.5},
		visual = "upright_sprite",
		visual_size = {x = 1, y = 1, z = 1},
		textures = {"soundstuff_racecar.png", "soundstuff_racecar.png^[transformFX"},
		static_save = false,
	},

	on_activate = function(self, _staticdata, _dtime_s)
		self.min_x = self.object:get_pos().x - drive_distance * 0.5
		self.max_x = self.min_x + drive_distance
		self.vel = vector.new(drive_speed, 0, 0)
	end,

	on_step = function(self, _dtime, _moveresult)
		local pos = self.object:get_pos()
		if pos.x < self.min_x then
			self.vel = vector.new(drive_speed, 0, 0)
		elseif pos.x > self.max_x then
			self.vel = vector.new(-drive_speed, 0, 0)
		end
		self.object:set_velocity(self.vel)
	end,
})
