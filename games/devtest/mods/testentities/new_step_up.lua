-- This entity helps test/showcase new_step_up for lua entities.
local setting_cycle = {legacy = 1, floaty = 2, rigid = 3}
local settings = {"legacy", "floaty", "rigid"}
core.register_entity("testentities:step_up_test", {
	initial_properties = {
		physical = true,
		collide_with_objects = false,
		visual = "cube",
		textures = { "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png" },
		infotext = "Punch me to toggle new_step_up.\nnew_step_up fixes the jolt when jumping up a node.\nSetting: legacy",
		new_step_up = 0,
		stepheight = 0.5,
		-- This collision box is set like this for testing.
		visual_size = { x = 0.8, y = 0.8, z = 0.8 },
		collisionbox = { -0.4, -0.4, -0.4, 0.4, 0.4, 0.4 }
	},
	on_activate = function(self, staticdata, dtime_s)
		self.object:set_acceleration(vector.new(0, -10, 0))
		self.object:set_armor_groups({punch_operable = 1})
	end,
	on_punch = function(self)
		local new_setting_id = setting_cycle[self.object:get_properties().new_step_up] + 1
		if new_setting_id > #settings then new_setting_id = 1 end
		local new_setting = settings[new_setting_id]
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
