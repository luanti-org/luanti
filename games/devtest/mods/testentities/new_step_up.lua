-- This entity helps test/showcase new_step_up for lua entities.
core.register_entity("testentities:step_up_test", {
	initial_properties = {
		physical = true,
		collide_with_objects = false,
		visual = "cube",
		textures = { "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png" },
		infotext = "Punch me to toggle new_step_up.\nnew_step_up fixes the jolt when jumping up a node.\nSetting:" .. tostring(0),
		new_step_up = 0,
		stepheight = 0.5,
	},
	on_activate = function(self, staticdata, dtime_s)
		self.object:set_acceleration(vector.new(0, -10, 0))
		self.object:set_armor_groups({punch_operable = 1})
	end,
	on_punch = function(self)
		local new_setting = self.object:get_properties().new_step_up + 1
		if new_setting > 2 then new_setting = 0 end
		self.object:set_properties({
			new_step_up = new_setting,
			infotext = "Punch me to toggle new_step_up.\nnew_step_up fixes the jolt when jumping up a node.\nSetting:" .. tostring(new_setting)
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
