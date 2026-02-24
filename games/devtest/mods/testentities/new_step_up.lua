core.register_entity("testentities:step_up_sniffer", {
	initial_properties = {
		physical = true,
		collide_with_objects = false,
		visual = "cube",
		textures = { "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png" },
	},
	on_activate = function(self, staticdata, dtime_s)
		self.object:set_acceleration(vector.new(0, -10, 0))
		self.object:set_properties({
			-- This setting.
			new_step_up = true,
			stepheight = 0.5,
		})
	end,
	on_step = function(self, dtime, moveresult)
		local touching_ground = moveresult.touching_ground
		local x_vel = self.object:get_velocity().x
		if x_vel < 1 then
			self.object:add_velocity(vector.new(0.05, 0, 0))
		end
		if touching_ground and x_vel == 0 then
			self.object:add_velocity(vector.new(0, 5, 0))
		end
	end,
})
